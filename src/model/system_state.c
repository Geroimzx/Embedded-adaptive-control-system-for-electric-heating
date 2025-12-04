#include "model/system_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdbool.h>

// Статична змінна, доступна тільки всередині цього файлу.
static sensors_state_t g_system_state;

// М'ютекс для захисту доступу до g_system_state
static SemaphoreHandle_t g_state_mutex;

void system_state_init(void) {
    g_state_mutex = xSemaphoreCreateMutex();
    memset(&g_system_state, 0, sizeof(sensors_state_t));
    g_system_state.temperature_c_sensor1 = -999.0f;
    g_system_state.temperature_c_sensor2 = -999.0f;
    g_system_state.relay_is_on = false;
    g_system_state.ui_state = UI_STATE_SPLASH_SCREEN;
}

void system_state_get(sensors_state_t *state_copy) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        *state_copy = g_system_state;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_temp_s1(float temp) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.temperature_c_sensor1 = temp;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_temp_s2(float temp) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.temperature_c_sensor2 = temp;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_temp_outside(float temp) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.temperature_c_outside = temp;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_current_setpoint(float setpoint) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.current_setpoint = setpoint;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_wifi_connected(bool new_state) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.wifi_connected = new_state;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_presence_state(bool presence) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.presence_state = presence;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_relay_state(bool relay_on) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.relay_is_on = relay_on;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_system_state(system_state_t new_state) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.system_state = new_state;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_ui_state(ui_state_t new_state) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.ui_state = new_state;
        xSemaphoreGive(g_state_mutex);
    }
}

void system_state_set_error_code(int error_code) {
    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.error_code = error_code;
        xSemaphoreGive(g_state_mutex);
    }
}