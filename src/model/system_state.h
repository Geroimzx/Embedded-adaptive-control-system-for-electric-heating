#ifndef COMPONENTS_SYSTEM_STATE_H_
#define COMPONENTS_SYSTEM_STATE_H_

#include "esp_err.h"
#include <stdbool.h>
#include "view/display_manager.h"
#include "model/main_control.h"


// Структура, що описує всі спільні дані нашої системи
typedef struct {
    float temperature_c_sensor1;
    float temperature_c_sensor2;
    float temperature_c_outside;
    float current_setpoint;
    bool wifi_connected;
    bool relay_is_on;
    bool presence_state;
    system_state_t system_state;
    ui_state_t ui_state;
    int error_code;
} sensors_state_t;

/**
 * @brief Ініціалізує менеджер стану (створює м'ютекс).
 */
void system_state_init(void);

/**
 * @brief Потокобезпечно отримує копію поточного стану системи.
 *
 * @param[out] state_copy Вказівник на структуру, куди буде скопійовано стан.
 */
void system_state_get(sensors_state_t *state_copy);

/**
 * @brief Потокобезпечно встановлює нове значення температури для датчика 1.
 */
void system_state_set_temp_s1(float temp);

/**
 * @brief Потокобезпечно встановлює нове значення температури для датчика 2.
 */
void system_state_set_temp_s2(float temp);

/*
 * @brief Потокобезпечно встановлює нове значення зовнішньої температури.
*/
void system_state_set_temp_outside(float temp);

/*
 * @brief Потокобезпечно встановлює нове значення поточного уставленого значення.
*/
void system_state_set_current_setpoint(float setpoint);

// Встановлює поточний стан присутності
void system_state_set_presence_state(bool presence);

// Встановлює поточний стан wifi
void system_state_set_wifi_connected(bool new_state);

void system_state_set_relay_state(bool relay_on);

void system_state_set_system_state(system_state_t new_state);

// Встановлює поточний стан UI (викликається з контролерів, що керують логікою UI).
void system_state_set_ui_state(ui_state_t new_state);

void system_state_set_error_code(int error_code);

#endif /* COMPONENTS_SYSTEM_STATE_H_ */