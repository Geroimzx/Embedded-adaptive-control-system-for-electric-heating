#include "controller/sensor/presence_controller.h"
#include "drivers/sensor/hlk_ld2420_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "PRESENCE_CTRL";

// Черга FreeRTOS для обробки подій присутності
static QueueHandle_t presence_event_queue = NULL;

// Перелічення для типів подій
typedef enum {
    PRESENCE_EVENT_DETECTED,
    PRESENCE_EVENT_LOST
} presence_event_t;

// Обробник події, який викликається ISR
// Завдання ISR - лише поставити подію в чергу
static void presence_event_isr_handler(bool presence_detected) {
    presence_event_t event = presence_detected ? PRESENCE_EVENT_DETECTED : PRESENCE_EVENT_LOST;
    xQueueSendFromISR(presence_event_queue, &event, NULL);
}

// Задача FreeRTOS для обробки подій з черги
static void presence_task(void *args) {
    presence_event_t event;
    while (1) {
        if (xQueueReceive(presence_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event) {
                case PRESENCE_EVENT_DETECTED:
                    ESP_LOGI(TAG, "!!! ПРИСУТНІСТЬ ВИЯВЛЕНО !!!");
                    system_state_set_presence_state(true);
                    break;
                case PRESENCE_EVENT_LOST:
                    ESP_LOGI(TAG, "--- ПРИСУТНІСТЬ ВТРАЧЕНО ---");
                    system_state_set_presence_state(false);
                    break;
            }
        }
    }
}

esp_err_t presence_controller_init(gpio_num_t presence_sensor_gpio_num) {
    esp_err_t ret = ESP_OK;

    // Створюємо чергу для обробки подій
    presence_event_queue = xQueueCreate(10, sizeof(presence_event_t));
    if (presence_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create presence event queue.");
        return ESP_FAIL;
    }

    // Ініціалізуємо драйвер, передавши йому колбек-функцію
    ret = hlk_ld2420_driver_init(presence_sensor_gpio_num, presence_event_isr_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HLK-LD2420 driver.");
        return ret;
    }

    // Створюємо задачу FreeRTOS, яка буде обробляти події з черги
    xTaskCreate(presence_task, "presence_task", 2048, NULL, 5, NULL);

    system_state_set_presence_state(hlk_ld2420_is_present());
    ESP_LOGI(TAG, "Presence controller initialized.");
    return ESP_OK;
}