#ifndef COMPONENTS_CONTROLLER_RELAY_CONTROLLER_H_
#define COMPONENTS_CONTROLLER_RELAY_CONTROLLER_H_

#include "esp_err.h"
#include <stdbool.h>
#include "driver/gpio.h"

/**
 * @brief Ініціалізує контролер реле та базовий драйвер реле.
 *
 * @param heater_gpio_num Номер GPIO піна для реле обігрівача.
 * @param heater_active_level Логічний рівень, який активує реле обігрівача (1 для HIGH, 0 для LOW).
 * @return esp_err_t ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t relay_controller_init(gpio_num_t heater_gpio_num, int heater_active_level);

/**
 * @brief Встановлює бажаний стан обігрівача (увімкнено/вимкнено).
 * Ця функція оновлює фактичний стан реле через relay_driver.
 *
 * @param state true для увімкнення обігрівача, false для вимкнення.
 * @return esp_err_t ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t relay_controller_set_heater_state(bool state);

/**
 * @brief Отримує поточний бажаний стан обігрівача.
 *
 * @return true, якщо обігрівач має бути увімкнений, false, якщо вимкнений.
 */
bool relay_controller_get_heater_state(void);

#endif /* COMPONENTS_CONTROLLER_RELAY_CONTROLLER_H_ */