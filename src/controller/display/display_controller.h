#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include "esp_err.h"
#include "driver/gpio.h"

/**
 * @brief Ініціалізує контролер дисплея.
 * @return esp_err_t ESP_OK у разі успіху.
 */
esp_err_t display_controller_init(gpio_num_t sda_gpio, gpio_num_t scl_gpio);

/**
 * @brief Оновлює дані, що відображаються на екрані.
 */
void display_controller_update(void);

#endif // DISPLAY_CONTROLLER_H