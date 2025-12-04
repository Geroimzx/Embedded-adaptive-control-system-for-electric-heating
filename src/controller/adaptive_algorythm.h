#ifndef ADAPTIVE_ALGORYTHM_H
#define ADAPTIVE_ALGORYTHM_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Ініціалізує адаптивний алгоритм (NVS, фонова задача навчання).
 */
esp_err_t adaptive_thermo_init(void);

/**
 * @brief Передає дані з сенсорів в алгоритм для аналізу.
 */
void adaptive_thermo_notify_sensor(float room_temp, float outside_temp, bool presence, int weekday, int hour, int minute);

/**
 * @brief (Опціонально) Налаштування швидкості нагріву приміщення (градусів на годину).
 * За замовчуванням 3.0.
 */
void adaptive_thermo_set_heat_rate(float degrees_per_hour);

/**
 * @brief Зупиняє алгоритм і зберігає стан.
 */
void adaptive_thermo_deinit(void);

/**
 * @brief Повертає розраховану цільову температуру (setpoint).
 * Саме це значення ти використовуєш для свого PID.
 */
float adaptive_thermo_get_setpoint(void);

#endif