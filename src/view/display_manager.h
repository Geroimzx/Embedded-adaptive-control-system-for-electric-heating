#pragma once

#include <stdbool.h>
#include "driver/gpio.h"
#include "model/main_control.h"

typedef enum {
    UI_STATE_SPLASH_SCREEN,
    UI_STATE_MAIN_SCREEN,
    UI_STATE_SELECT_MENU,
    UI_STATE_TEMPERATURE_SELECT,
    UI_STATE_INFO,
    UI_STATE_EMERGENCY
} ui_state_t;

typedef struct {
    float temperature;           // Дані з термістора
    bool wifi_connected;        // Показувати тільки якщо є
    const char* time_str;       // Форматований час "HH:MM"
    const char* date_str;       // Форматована дата "DD/MM/YYYY"
    const char* mode_str;       // Назва режиму: "OFF", "MANUAL"
} main_screen_data_t;

void display_manager_init(gpio_num_t PIN_SDA, gpio_num_t PIN_SCL);
void render_splash_screen(void);
void render_main_screen(const main_screen_data_t* data);
void render_mode_select_screen(system_state_t preview_state);
void render_temperature_select_screen(float current_temp);
void render_info_screen(const char* ap_ssid, const char* ap_pass, const char* ip_addr);
void render_emergency_screen(int error_code);