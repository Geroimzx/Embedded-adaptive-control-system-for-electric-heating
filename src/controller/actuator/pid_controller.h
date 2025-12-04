#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdint.h>

/**
 * @brief Структура для зберігання стану та налаштувань ПІД-регулятора.
 */
typedef struct {
    /* Коефіцієнти регулятора */
    float kp; // Пропорційний
    float ki; // Інтегральний
    float kd; // Диференціальний

    /* Межі вихідного сигналу */
    float out_min; // Мінімальне значення (напр. 0)
    float out_max; // Максимальне значення (напр. 100)

    /* Внутрішні змінні стану (не змінювати вручну) */
    float _integral;      // Накопичена інтегральна сума
    float _pre_error;     // Помилка на попередньому кроці
    int64_t _last_time_us; // Час останнього розрахунку в мікросекундах
} pid_controller_t;

/**
 * @brief Ініціалізує ПІД-регулятор.
 *
 * @param pid Вказівник на структуру pid_controller_t.
 * @param kp Пропорційний коефіцієнт.
 * @param ki Інтегральний коефіцієнт.
 * @param kd Диференціальний коефіцієнт.
 * @param out_min Мінімальне значення вихідного сигналу.
 * @param out_max Максимальне значення вихідного сигналу.
 */
void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float out_min, float out_max);

/**
 * @brief Розраховує керуючий сигнал ПІД-регулятора.
 * 
 * Цю функцію потрібно викликати періодично.
 *
 * @param pid Вказівник на ініціалізовану структуру pid_controller_t.
 * @param setpoint Бажане значення (цільова температура).
 * @param measured_value Поточне виміряне значення (температура в приміщенні).
 * @return float Розрахований керуючий сигнал в межах [out_min, out_max].
 */
float pid_compute(pid_controller_t *pid, float setpoint, float measured_value);

#endif // PID_CONTROLLER_H