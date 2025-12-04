#ifndef BUTTON_CONTROLLER_H
#define BUTTON_CONTROLLER_H

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdbool.h>

/**
 * @brief Структура для конфігурації однієї кнопки.
 * Використовується для передачі параметрів ініціалізації.
 */
typedef struct {
    gpio_num_t gpio_num;              // Номер GPIO піна кнопки
    uint32_t long_press_threshold_ms; // Поріг часу (мс) для визначення утримання (0 для вимкнення)
} button_controller_button_config_t;

/**
 * @brief Ініціалізує контролер кнопок та базовий драйвер кнопок.
 * Створює задачу FreeRTOS для обробки подій кнопок.
 *
 * @param configs Масив структур конфігурацій для кожної кнопки.
 * @param num_buttons Кількість кнопок у масиві configs.
 * @return esp_err_t ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t button_controller_init(const button_controller_button_config_t configs[], size_t num_buttons);

#endif /* BUTTON_CONTROLLER_H */