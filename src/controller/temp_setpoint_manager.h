#ifndef TEMP_SETPOINT_MANAGER_H
#define TEMP_SETPOINT_MANAGER_H

#include "esp_err.h"

/**
 * @brief Ініціалізує менеджер заданої температури.
 * 
 * Читає останнє збережене значення з NVS. Якщо значення не знайдено
 * (перший запуск), встановлює значення за замовчуванням.
 * Цю функцію потрібно викликати один раз при старті програми.
 * 
 * @return esp_err_t результат ініціалізації.
 */
esp_err_t temp_setpoint_manager_init(void);

/**
 * @brief Встановлює нове значення заданої температури.
 * 
 * Оновлює значення в оперативній пам'яті та зберігає його в NVS
 * для використання після перезавантаження.
 * 
 * @param new_temp Нова температура (float).
 */
void temp_setpoint_manager_set(float new_temp);

/**
 * @brief Отримує поточне значення заданої температури.
 * 
 * Повертає значення з оперативної пам'яті для швидкого доступу.
 * 
 * @return float Поточна задана температура.
 */
float temp_setpoint_manager_get(void);

#endif // TEMP_SETPOINT_MANAGER_H