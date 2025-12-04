#ifndef PRESENCE_CONTROLLER_H
#define PRESENCE_CONTROLLER_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "model/system_state.h"

/**
 * @brief Ініціалізує контролер присутності.
 *
 * @param presence_sensor_gpio_num Номер GPIO піна датчика присутності.
 * 
 * @return esp_err_t ESP_OK у разі успіху.
 */
esp_err_t presence_controller_init(gpio_num_t presence_sensor_gpio_num);

#endif // PRESENCE_CONTROLLER_H