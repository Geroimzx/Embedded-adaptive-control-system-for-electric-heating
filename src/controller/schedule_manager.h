#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include "esp_err.h"
#include <stdint.h>

#define MAX_SCHEDULE_POINTS_PER_DAY 4
#define DAYS_IN_WEEK 7

// Структура для однієї точки в розкладі (час + температура)
typedef struct {
    uint8_t hour;       // Година (0-23)
    uint8_t minute;     // Хвилина (0-59)
    float temperature;  // Бажана температура
} schedule_point_t;

// Структура для розкладу на один день
typedef struct {
    schedule_point_t points[MAX_SCHEDULE_POINTS_PER_DAY];
    uint8_t num_points; // Кількість активних точок для цього дня
} day_schedule_t;

// Структура для повного тижневого розкладу
typedef struct {
    day_schedule_t days[DAYS_IN_WEEK]; // 0 = Неділя, 1 = Понеділок, ..., 6 = Субота
} week_schedule_t;

/**
 * @brief Ініціалізує менеджер розкладу.
 * 
 * Завантажує розклад з NVS. Якщо розклад не знайдено (перший запуск),
 * створює та зберігає розклад за замовчуванням.
 * 
 * @return esp_err_t Результат ініціалізації.
 */
esp_err_t schedule_manager_init(void);

/**
 * @brief Отримує задану температуру згідно з поточним часом та розкладом.
 * 
 * Це головна функція, яку викликає головний цикл керування.
 * 
 * @return float Бажана температура на поточний момент.
 */
float schedule_manager_get_current_setpoint(void);

/**
 * @brief Зберігає новий тижневий розклад в NVS.
 * 
 * @param schedule Вказівник на структуру з новим розкладом.
 * @return esp_err_t Результат збереження.
 */
esp_err_t schedule_manager_save_schedule(const week_schedule_t* schedule);

void schedule_manager_get_full(week_schedule_t *out);


#endif