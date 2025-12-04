#include "controller/sensor/temp_controller.h"
#include "drivers/sensor/temp_sensor_driver.h"
#include "model/system_state.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "hal/adc_types.h"
#include "hw_config.h"

static const char *TAG = "TEMP_CONTROLLER";

// Налаштування для передискретизації (Oversampling)
// Скільки разів зчитується значення датчика за один цикл для усереднення.
// Чим більше значення, тим краще згладжування шуму, але довше триває вимірювання.
// 16-64 оптимальний діапазоном.
#define OVERSAMPLING_COUNT 32

// Налаштування для експоненційного фільтра (EMA)
// Коефіцієнт згладжування. Чим ближче до 0, тим сильніше згладжування (повільніша реакція).
// Чим ближче до 1, тим слабше згладжування (швидша реакція).
// 0.1 - оптимальний вибір для повільно змінюваних даних, як температура.
#define EMA_FILTER_ALPHA 0.1f

// Статичні змінні для зберігання відфільтрованих значень між викликами
static float s_filtered_temp[NUM_TEMP_SENSORS];

static ntc_sensor_handle_t s_sensor_handles[NUM_TEMP_SENSORS];
static TaskHandle_t s_task_handle = NULL;
static uint32_t s_update_interval_ms = 1000;

/**
 * @brief Приватна функція для читання та усереднення температури з датчика.
 * Перший рівень фільтрації.
 */
static esp_err_t read_averaged_temperature(temp_sensor_id_t sensor_id, float *avg_temp) {
    float temp_sum = 0;
    int valid_samples = 0;
    float current_temp;

    for (int i = 0; i < OVERSAMPLING_COUNT; i++) {
        if (temp_sensor_driver_read_ntc(s_sensor_handles[sensor_id], &current_temp) == ESP_OK) {
            if (!isnan(current_temp)) {
                temp_sum += current_temp;
                valid_samples++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    if (valid_samples > 0) {
        *avg_temp = temp_sum / valid_samples;
        return ESP_OK;
    }

    return ESP_FAIL;
}

/**
 * @brief Приватна функція для ініціалізації та оновлення експоненційного фільтра.
 * Другий рівень фільтрації.
 */
static void update_ema_filter(temp_sensor_id_t sensor_id, float raw_temp) {
    if (isnan(s_filtered_temp[sensor_id])) {
        s_filtered_temp[sensor_id] = raw_temp;
    } else {
        // Формула фільтра: new_value = alpha * new_sample + (1 - alpha) * old_value
        s_filtered_temp[sensor_id] = (EMA_FILTER_ALPHA * raw_temp) + ((1.0f - EMA_FILTER_ALPHA) * s_filtered_temp[sensor_id]);
    }
}


static void temp_controller_task(void *pvParameters) {
    ESP_LOGI(TAG, "Task started.");
    float raw_averaged_temp;

    while (1) {
        // Обробка датчика 1
        if (read_averaged_temperature(TEMP_SENSOR_1, &raw_averaged_temp) == ESP_OK) {
            // Застосування другого рівня фільтрації
            update_ema_filter(TEMP_SENSOR_1, raw_averaged_temp);
            // Оновлення стану відфільтрованим значенням
            system_state_set_temp_s1(s_filtered_temp[TEMP_SENSOR_1]);
            ESP_LOGD(TAG, "Sensor 1: Raw=%.2f C, Filtered=%.2f C", raw_averaged_temp, s_filtered_temp[TEMP_SENSOR_1]);
        } else {
            ESP_LOGW(TAG, "Failed to read from Sensor 1");
        }

        // Обробка датчика 2
        if (read_averaged_temperature(TEMP_SENSOR_2, &raw_averaged_temp) == ESP_OK) {
            // Застосування другого рівня фільтрації
            update_ema_filter(TEMP_SENSOR_2, raw_averaged_temp);
            // Оновлення стану відфільтрованим значенням
            system_state_set_temp_s2(s_filtered_temp[TEMP_SENSOR_2]);
            ESP_LOGD(TAG, "Sensor 2: Raw=%.2f C, Filtered=%.2f C", raw_averaged_temp, s_filtered_temp[TEMP_SENSOR_2]);
        } else {
            ESP_LOGW(TAG, "Failed to read from Sensor 2");
        }

        vTaskDelay(pdMS_TO_TICKS(s_update_interval_ms));
    }
}

esp_err_t temp_controller_init(const temp_controller_config_t *config) {
    ESP_LOGI(TAG, "Initializing...");

    // Ініціалізація початкових значень для фільтрів
    for (int i = 0; i < NUM_TEMP_SENSORS; i++) {
        s_filtered_temp[i] = NAN;
    }

    // Ініціалізація драйверів
    ntc_thermistor_config_t ntc_config_1 = {.nominal_resistance = 10000.0f, .nominal_temperature_c = 25.0f, .b_value = 3950.0f, .fixed_resistor_ohms = 10000.0f};
    s_sensor_handles[TEMP_SENSOR_1] = temp_sensor_driver_init_ntc(THERMISTOR_ENVIRONMENT, ADC_ATTEN_DB_12, ADC_BITWIDTH_12, &ntc_config_1);
    if (!s_sensor_handles[TEMP_SENSOR_1]) return ESP_FAIL;

    ntc_thermistor_config_t ntc_config_2 = {.nominal_resistance = 10000.0f, .nominal_temperature_c = 25.0f, .b_value = 3950.0f, .fixed_resistor_ohms = 10000.0f};
    s_sensor_handles[TEMP_SENSOR_2] = temp_sensor_driver_init_ntc(THERMISTOR_RADIATOR, ADC_ATTEN_DB_12, ADC_BITWIDTH_12, &ntc_config_2);
    if (!s_sensor_handles[TEMP_SENSOR_2]) return ESP_FAIL;

    if (config) {
        s_update_interval_ms = config->update_interval_ms;
    }

    BaseType_t status = xTaskCreate(
        temp_controller_task,
        "temp_ctrl_task",
        config ? config->task_stack_size : 4096,
        NULL,
        config ? config->task_priority : 5,
        &s_task_handle
    );

    if (status != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initialized and task started successfully.");
    return ESP_OK;
}