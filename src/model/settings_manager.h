#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Основна структура налаштувань
typedef struct {
    struct {
        char sta_ssid[32];
        char sta_pass[64];
        char ap_ssid[32];
        char ap_pass[64];
    } wifi;
    
    struct {
        char host[64];
        int port;
        char token[32];
    } mqtt;
    
    struct {
        float lat;
        float lon;
        int interval_min;
    } geo;
    
    struct {
        struct { float kp; float ki; float kd; } pid;
        struct { 
            float rad_max; 
            float room_min; 
            float room_max; 
        } limits;
        int pwm_cycle_s;
    } control;
    
    char timezone[64];
    
    // Магічне число, щоб перевірити, чи наша структура не змінилась
    uint32_t magic_id; 
} app_settings_t;

/**
 * @brief Ініціалізація налаштувань.
 * Читає з NVS. Якщо пусто — записує дефолтні.
 */
esp_err_t settings_init(void);

/**
 * @brief Отримати вказівник на поточні налаштування (Read-only)
 */
const app_settings_t* settings_get(void);

/**
 * @brief Отримати вказівник на налаштування для редагування
 * Після редагування обов'язково викличте settings_save()
 */
app_settings_t* settings_get_writeable(void);

/**
 * @brief Зберегти поточні налаштування з RAM в NVS
 */
esp_err_t settings_save(void);

/**
 * @brief Скинути налаштування до заводських (тих, що в коді)
 */
esp_err_t settings_factory_reset(void);