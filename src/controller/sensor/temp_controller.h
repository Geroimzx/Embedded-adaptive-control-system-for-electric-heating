#ifndef COMPONENTS_CONTROLLERS_TEMP_CONTROLLER_H_
#define COMPONENTS_CONTROLLERS_TEMP_CONTROLLER_H_

#include "esp_err.h"

// Конфігурація для завдання контролера
typedef struct {
    uint32_t update_interval_ms; // Інтервал оновлення даних у мілісекундах
    int task_priority;           // Пріоритет завдання FreeRTOS
    int task_stack_size;         // Розмір стеку для завдання
} temp_controller_config_t;

// Перелік датчиків для внутрішнього використання
typedef enum {
    TEMP_SENSOR_1,
    TEMP_SENSOR_2,
    NUM_TEMP_SENSORS
} temp_sensor_id_t;

/**
 * @brief Ініціалізує контролер температури та запускає фонове завдання для оновлення даних.
 *
 * @param config Вказівник на структуру конфігурації.
 * @return esp_err_t ESP_OK у разі успіху.
 */
esp_err_t temp_controller_init(const temp_controller_config_t *config);

#endif /* COMPONENTS_CONTROLLERS_TEMP_CONTROLLER_H_ */