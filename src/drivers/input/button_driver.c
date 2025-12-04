#include "drivers/input/button_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/queue.h"

static const char *TAG = "button_driver";

#define DEBOUNCE_TIME_MS 50

static QueueHandle_t s_button_event_queue = NULL;

typedef struct {
    gpio_num_t gpio_num;
    uint32_t long_press_threshold_ms;
    esp_timer_handle_t long_press_timer_handle;
    esp_timer_handle_t debounce_timer_handle;
    int button_idx;
    bool is_pressed;
} button_info_t;

static button_info_t s_buttons[MAX_BUTTONS];
static int s_num_configured_buttons = 0;
static bool s_driver_initialized = false;

static void button_isr_handler(void* arg);
static void long_press_timer_callback(void* arg);
static void debounce_timer_callback(void* arg);

esp_err_t button_driver_init(const button_config_t configs[], size_t num_buttons) {
    if (s_driver_initialized) {
        ESP_LOGW(TAG, "Button driver already initialized. Re-initialization is not supported.");
        return ESP_FAIL;
    }
    if (num_buttons == 0 || num_buttons > MAX_BUTTONS) {
        ESP_LOGE(TAG, "Invalid number of buttons (%zu). Must be between 1 and %d.", num_buttons, MAX_BUTTONS);
        return ESP_ERR_INVALID_ARG;
    }

    s_num_configured_buttons = num_buttons;

    s_button_event_queue = xQueueCreate(10, sizeof(button_event_t));
    if (s_button_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button event queue");
        return ESP_FAIL;
    }

    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
        return err;
    }

    for (int i = 0; i < s_num_configured_buttons; i++) {
        s_buttons[i].gpio_num = configs[i].gpio_num;
        s_buttons[i].long_press_threshold_ms = configs[i].long_press_threshold_ms;
        s_buttons[i].button_idx = i;
        s_buttons[i].is_pressed = false;

        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_ANYEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << s_buttons[i].gpio_num);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

        err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure GPIO %d for button %d: %s", s_buttons[i].gpio_num, i, esp_err_to_name(err));
            return err;
        }

        err = gpio_isr_handler_add(s_buttons[i].gpio_num, button_isr_handler, (void*)&s_buttons[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d (button %d): %s", s_buttons[i].gpio_num, i, esp_err_to_name(err));
            return err;
        }

        if (s_buttons[i].long_press_threshold_ms > 0) {
            char timer_name[20];
            snprintf(timer_name, sizeof(timer_name), "btn_long_press_%d", i);
            const esp_timer_create_args_t timer_args = {
                .callback = &long_press_timer_callback,
                .arg = (void*)&s_buttons[i],
                .name = timer_name
            };
            err = esp_timer_create(&timer_args, &s_buttons[i].long_press_timer_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create long press timer for button %d: %s", i, esp_err_to_name(err));
                return err;
            }
        } else {
            s_buttons[i].long_press_timer_handle = NULL;
        }

        char debounce_timer_name[20];
        snprintf(debounce_timer_name, sizeof(debounce_timer_name), "btn_debounce_%d", i);
        const esp_timer_create_args_t debounce_timer_args = {
            .callback = &debounce_timer_callback,
            .arg = (void*)&s_buttons[i],
            .name = debounce_timer_name
        };
        err = esp_timer_create(&debounce_timer_args, &s_buttons[i].debounce_timer_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create debounce timer for button %d: %s", i, esp_err_to_name(err));
            return err;
        }
    }

    s_driver_initialized = true;
    ESP_LOGI(TAG, "Button driver successfully initialized for %d buttons.", s_num_configured_buttons);
    return ESP_OK;
}

QueueHandle_t button_driver_get_event_queue(void) {
    return s_button_event_queue;
}

static void IRAM_ATTR button_isr_handler(void* arg) {
    button_info_t *btn_info = (button_info_t *)arg;
    
    esp_timer_start_once(btn_info->debounce_timer_handle, DEBOUNCE_TIME_MS * 1000);
}

static void debounce_timer_callback(void* arg) {
    button_info_t *btn_info = (button_info_t *)arg;
    int gpio_level = gpio_get_level(btn_info->gpio_num);
    button_event_t event;
    event.gpio_num = btn_info->gpio_num;

    if (gpio_level == 0) {
        if (!btn_info->is_pressed) {
            btn_info->is_pressed = true;
            if (btn_info->long_press_timer_handle != NULL) {
                esp_timer_start_once(btn_info->long_press_timer_handle, btn_info->long_press_threshold_ms * 1000);
            }
        }
    } else {
        if (btn_info->is_pressed) {
            btn_info->is_pressed = false;

            if (btn_info->long_press_timer_handle != NULL && esp_timer_is_active(btn_info->long_press_timer_handle)) {
                esp_timer_stop(btn_info->long_press_timer_handle);
                event.event = BUTTON_EVENT_PRESSED;
                if (xQueueSend(s_button_event_queue, &event, 0) == pdFALSE) {
                    ESP_LOGE(TAG, "Failed to send short press event to queue from timer");
                }
            } else {
                event.event = BUTTON_EVENT_LONG_PRESS_END;
                if (xQueueSend(s_button_event_queue, &event, 0) == pdFALSE) {
                    ESP_LOGE(TAG, "Failed to send long press end event to queue from timer");
                }
            }
        }
    }
}

static void long_press_timer_callback(void* arg) {
    button_info_t *btn_info = (button_info_t *)arg;
    button_event_t event;
    event.gpio_num = btn_info->gpio_num;

    if (gpio_get_level(btn_info->gpio_num) == 0) {
        event.event = BUTTON_EVENT_LONG_PRESS_START;
        if (xQueueSend(s_button_event_queue, &event, 0) == pdFALSE) {
            ESP_LOGE(TAG, "Failed to send long press event to queue from timer");
        }
    }
}
