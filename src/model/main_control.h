#ifndef MAIN_CONTROL_H
#define MAIN_CONTROL_H

#include "freertos/FreeRTOS.h"

typedef enum {
    STATE_BOOT,
    STATE_OFF,
    STATE_MANUAL,
    STATE_ADAPTIVE,
    STATE_PROGRAMMED,
    STATE_ANTI_FREEZE,
    STATE_EMERGENCY,
    STATE_MODE_SELECT
} system_state_t;


/**
 * @brief Потокобезпечно змінює глобальний стан системи.
 * 
 * @param new_state Новий стан, в який потрібно перейти.
 */
void main_control_change_state(system_state_t new_state);

/**
 * @brief Потокобезпечно отримує поточний стан системи.
 * 
 * @return system_state_t Поточний стан.
 */
system_state_t main_control_get_state(void);

/**
 * @brief Переключає режим для попереднього перегляду у стані STATE_MODE_SELECT.
 * 
 * @param go_up true для переключення "вгору", false для "вниз".
 */
void main_control_cycle_preview_mode(bool go_up);

system_state_t main_control_get_preview_state(void);

/**
 * @brief Повертає назву стану у вигляді рядка.
 * 
 * @param state Стан системи.
 * @return const char* Рядок з назвою.
 */
const char* state_to_string(system_state_t state);

#endif // MAIN_CONTROL_H