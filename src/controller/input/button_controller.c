#include "controller/input/button_controller.h"
#include "drivers/input/button_driver.h"
#include "hw_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "model/main_control.h"
#include "controller/temp_setpoint_manager.h"
#include "model/system_state.h"

static const char *TAG = "button_controller";

#define TEMP_SELECT_SCREEN_TIMEOUT_MS 3000 
#define LOOP_TIMEOUT_MS 100 

static void button_event_handler_task(void *pvParameters);

esp_err_t button_controller_init(const button_controller_button_config_t configs[], size_t num_buttons) {
    ESP_LOGI(TAG, "Ініціалізація контролера кнопок...");

    if (num_buttons > MAX_BUTTONS) {
        ESP_LOGE(TAG, "Забагато кнопок для драйвера (макс. %d).", MAX_BUTTONS);
        return ESP_ERR_INVALID_ARG;
    }

    button_config_t driver_configs[MAX_BUTTONS];
    for (size_t i = 0; i < num_buttons; i++) {
        driver_configs[i].gpio_num = configs[i].gpio_num;
        driver_configs[i].long_press_threshold_ms = configs[i].long_press_threshold_ms;
    }

    esp_err_t err = button_driver_init(driver_configs, num_buttons);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не вдалося ініціалізувати драйвер кнопок: %s", esp_err_to_name(err));
        return err;
    }

    if (xTaskCreate(button_event_handler_task, "ButtonEventTask", 4096, NULL, 10, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Не вдалося створити задачу обробника подій кнопок.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Контролер кнопок ініціалізовано.");
    return ESP_OK;
}

static void button_event_handler_task(void *pvParameters) {
    button_event_t event;
    QueueHandle_t button_queue = button_driver_get_event_queue();

    int64_t last_activity_time = esp_timer_get_time();
    bool is_in_temp_select_mode = false;
    bool is_in_info_mode = false;

    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Черга подій кнопок NULL, задача завершується.");
        vTaskDelete(NULL);
        return;
    }

    for (;;) {
        if (xQueueReceive(button_queue, &event, pdMS_TO_TICKS(LOOP_TIMEOUT_MS)) == pdTRUE) {
            
            last_activity_time = esp_timer_get_time(); 
            system_state_t current_system_state = main_control_get_state();

            switch (event.gpio_num) {
                case GPIO_BUTTON_UP_PIN:
                    if (event.event == BUTTON_EVENT_PRESSED) {
                        ESP_LOGI(TAG, "Кнопка ВГОРУ");
                        if (current_system_state == STATE_MANUAL) {
                            system_state_set_ui_state(UI_STATE_TEMPERATURE_SELECT);
                            is_in_temp_select_mode = true;

                            float current_temp = temp_setpoint_manager_get();
                            temp_setpoint_manager_set(current_temp + 0.25f);
                        } else if (current_system_state == STATE_MODE_SELECT) {
                            main_control_cycle_preview_mode(true);
                        }
                    } else if (event.event == BUTTON_EVENT_LONG_PRESS_START) {
                        if (current_system_state == STATE_OFF && !is_in_info_mode) {
                            is_in_info_mode = true;
                            system_state_set_ui_state(UI_STATE_INFO);
                        } else {
                            is_in_info_mode = false;
                            system_state_set_ui_state(UI_STATE_MAIN_SCREEN);
                            main_control_change_state(STATE_OFF);
                        }
                    }
                    break;

                case GPIO_BUTTON_DOWN_PIN:
                    if (event.event == BUTTON_EVENT_PRESSED) {
                        ESP_LOGI(TAG, "Кнопка ВНИЗ");
                        if (current_system_state == STATE_MANUAL) {
                            system_state_set_ui_state(UI_STATE_TEMPERATURE_SELECT);
                            is_in_temp_select_mode = true;

                            float current_temp = temp_setpoint_manager_get();
                            temp_setpoint_manager_set(current_temp - 0.25f);
                        } else if (current_system_state == STATE_MODE_SELECT) {
                            main_control_cycle_preview_mode(false);
                        }
                    } else if (event.event == BUTTON_EVENT_LONG_PRESS_START) {
                        if (current_system_state != STATE_MODE_SELECT) {
                            main_control_change_state(STATE_MODE_SELECT);
                        }
                    }
                    break;
                
                default:
                    break;
            }
        }

        if (is_in_temp_select_mode) {
            int64_t current_time = esp_timer_get_time();
            if ((current_time - last_activity_time) / 1000 > TEMP_SELECT_SCREEN_TIMEOUT_MS) {
                ESP_LOGI(TAG, "Таймаут неактивності: повернення на головний екран");
                system_state_set_ui_state(UI_STATE_MAIN_SCREEN);
                is_in_temp_select_mode = false;
            }
        } 
    }
}