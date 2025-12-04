#ifndef COMPONENTS_DRIVERS_RELAY_DRIVER_H_
#define COMPONENTS_DRIVERS_RELAY_DRIVER_H_

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdbool.h>

/**
 * @brief Ініціалізує драйвер реле.
 *
 * @param gpio_num Номер GPIO піна, до якого підключено реле.
 * @param active_level Логічний рівень, який активує реле (1 для HIGH, 0 для LOW).
 * @return esp_err_t ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t relay_driver_init(gpio_num_t gpio_num, int active_level);

/**
 * @brief Встановлює стан реле (увімкнено/вимкнено).
 *
 * @param state true для увімкнення реле, false для вимкнення.
 * @return esp_err_t ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t relay_driver_set_state(bool state);

/**
 * @brief Отримує поточний стан реле.
 *
 * @return true, якщо реле активне (увімкнене), false, якщо неактивне (вимкнене).
 */
bool relay_driver_get_state(void);

#endif /* COMPONENTS_DRIVERS_RELAY_DRIVER_H_ */