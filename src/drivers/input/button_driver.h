#ifndef COMPONENTS_DRIVERS_BUTTON_DRIVER_H_
#define COMPONENTS_DRIVERS_BUTTON_DRIVER_H_

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Максимальна кількість кнопок, які може підтримувати драйвер
#define MAX_BUTTONS 2

// Тип події, яку генерує кнопка
typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_PRESSED,           // Кнопка була натиснута (відпущена після короткого натискання)
    BUTTON_EVENT_RELEASED,          // НОВА ПОДІЯ: Кнопка була відпущена
    BUTTON_EVENT_LONG_PRESS_START,  // Кнопка була утримана довше порогу
    BUTTON_EVENT_LONG_PRESS_END,    // Кнопка була відпущена після утримання
} button_event_type_t;

// Структура, що описує подію кнопки
typedef struct {
    gpio_num_t gpio_num;            // GPIO пін, який генерував подію
    button_event_type_t event;      // Тип події
} button_event_t;

/**
 * @brief Структура конфігурації для однієї кнопки.
 */
typedef struct {
    gpio_num_t gpio_num;            // Номер GPIO піна
    uint32_t long_press_threshold_ms; // Поріг часу (мс) для визначення утримання (0 для вимкнення)
} button_config_t;

/**
 * @brief Ініціалізує драйвер кнопок для декількох кнопок.
 *
 * @param configs Масив структур конфігурацій для кожної кнопки.
 * @param num_buttons Кількість кнопок у масиві configs.
 * @return esp_err_t ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t button_driver_init(const button_config_t configs[], size_t num_buttons);

/**
 * @brief Отримує дескриптор черги подій кнопок.
 * Контролер буде читати події з цієї черги.
 *
 * @return QueueHandle_t Дескриптор черги, або NULL, якщо драйвер не ініціалізовано.
 */
QueueHandle_t button_driver_get_event_queue(void);

#endif /* COMPONENTS_DRIVERS_BUTTON_DRIVER_H_ */
