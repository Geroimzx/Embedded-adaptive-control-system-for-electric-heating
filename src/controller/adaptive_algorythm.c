#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "controller/adaptive_algorythm.h"
#include "model/settings_manager.h"

static const char *TAG = "adaptive_algo";

// ------------------------- Config: NVS ---------------------------
#define AT_NVS_NAMESPACE        "at_model"
#define NVS_KEY_BIAS            "bias"
#define NVS_KEY_MODEL           "model"
#define NVS_KEY_SETPOINT        "setpoint"

// ------------------------- Config: Time & Constants ---------------------------
#define DAYS_PER_WEEK           7
#define HOURS_PER_DAY           24
#define SECONDS_PER_MINUTE      60
#define MINUTES_PER_HOUR        60

#define SMOOTH_DELTA_PER_SEC    0.01f
#define SHORT_PRESENCE_SEC      20
#define BASE_PRESENCE_TIMEOUT   15
#define MIN_PRESENCE_TIMEOUT    6
#define MAX_PRESENCE_TIMEOUT    35

#define LEARNING_INTERVAL_SEC   600
#define LEARNING_RATE_POSITIVE  0.14f
#define LEARNING_RATE_NEGATIVE  0.06f
#define NVS_SAVE_INTERVAL_SEC   3600

#define PREHEAT_LOOKAHEAD_HOURS 3
#define PREHEAT_BIAS_THRESHOLD  0.6f
#define PREHEAT_BUFFER_MIN      15
#define PREHEAT_BOOST_DEG       0.5f

#define WEATHER_COLD_THRESH     -5.0f
#define WEATHER_COLD_GAIN       0.16f
#define WEATHER_COLD_MAX_COMP   0.8f

#define WEATHER_MILD_THRESH     10.0f
#define WEATHER_MILD_GAIN       -0.03f
#define WEATHER_MILD_MAX_COMP   -0.3f

typedef struct {
    float heat_rate_deg_per_h;
} thermal_model_t;

static float s_behavior_bias[DAYS_PER_WEEK][HOURS_PER_DAY];
static thermal_model_t s_thermal_model = { .heat_rate_deg_per_h = 3.0f };

static float s_room_temp = 20.0f;
static float s_outside_temp = 0.0f;
static bool  s_presence = false;
static int   s_weekday = 0;
static int   s_hour = 0;
static int   s_minute = 0;

static float s_current_setpoint = 21.0f;

static SemaphoreHandle_t s_lock = NULL;
static TaskHandle_t s_task = NULL;
static volatile bool s_task_running = true;

static int s_presence_true_seconds = 0;
static int s_presence_false_seconds = 0;
static bool s_presence_valid = false;
static int s_presence_timeout_min = BASE_PRESENCE_TIMEOUT;
static uint32_t s_presence_start_time_sec = 0;
static uint32_t s_seconds_counter = 0;
static bool s_preheat_active = false;

static bool s_bias_dirty = false;
static bool s_model_dirty = false;
static bool s_setpoint_dirty = false;

static float clampf(float v, float lo, float hi) {
    if(v<lo) return lo;
    if(v>hi) return hi;
    return v;
}

static esp_err_t nvs_save_blob_local(const char *key, const void *data, size_t size) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(AT_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if(err!=ESP_OK) {
        ESP_LOGW(TAG, "nvs_open RW failed for '%s': %s", key, esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(handle,key,data,size);
    if(err==ESP_OK) err=nvs_commit(handle);
    if(err!=ESP_OK) {
        ESP_LOGW(TAG, "nvs_set_blob/commit failed for '%s': %s", key, esp_err_to_name(err));
    }
    nvs_close(handle);
    return err;
}

static esp_err_t nvs_load_blob_local(const char *key, void *out, size_t size) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(AT_NVS_NAMESPACE, NVS_READONLY, &handle);
    if(err!=ESP_OK) {
        ESP_LOGW(TAG, "nvs_open RO failed for '%s': %s", key, esp_err_to_name(err));
        return err;
    }
    size_t required = 0;
    err = nvs_get_blob(handle, key, NULL, &required);
    if (err == ESP_OK) {
        if (required == size) {
            err = nvs_get_blob(handle, key, out, &required);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "nvs_get_blob read failed for '%s': %s", key, esp_err_to_name(err));
            }
        } else {
            ESP_LOGW(TAG, "NVS key '%s' has unexpected size %u (expected %u). Skipping load.", (const char*)key, (unsigned)required, (unsigned)size);
            err = ESP_ERR_NVS_NOT_FOUND;
        }
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_ERR_NVS_NOT_FOUND;
    } else {
        ESP_LOGW(TAG, "nvs_get_blob size query failed for '%s': %s", key, esp_err_to_name(err));
    }
    nvs_close(handle);
    return err;
}

static float compute_target_internal(void) {
    const app_settings_t *cfg = settings_get();
    float min_temp = cfg->control.limits.room_min;
    float comfort_temp = cfg->control.limits.room_max;

    int wd = s_weekday % DAYS_PER_WEEK;
    int h = s_hour % HOURS_PER_DAY;

    float bias = s_behavior_bias[wd][h];
    float target = min_temp + (comfort_temp - min_temp) * bias;

    float weather_corr = 0.0f;
    if(s_outside_temp <= WEATHER_COLD_THRESH) {
        weather_corr = clampf((WEATHER_COLD_THRESH - s_outside_temp) * WEATHER_COLD_GAIN, 0.0f, WEATHER_COLD_MAX_COMP);
    }
    else if(s_outside_temp >= WEATHER_MILD_THRESH) {
        weather_corr = clampf((WEATHER_MILD_THRESH - s_outside_temp) * WEATHER_MILD_GAIN, WEATHER_MILD_MAX_COMP, 0.0f);
    }
    target += weather_corr;

    if (s_presence_valid) {
        target = fmaxf(target, comfort_temp);
    }

    if (s_preheat_active) {
        target = comfort_temp + PREHEAT_BOOST_DEG;
    }

    target = clampf(target, min_temp, comfort_temp + PREHEAT_BOOST_DEG);

    return target;
}

static void adaptive_calc_task(void *arg) {
    TickType_t last_wake = xTaskGetTickCount();
    while(s_task_running){
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));

        if(xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE){
            s_seconds_counter++;
            int wd = s_weekday % DAYS_PER_WEEK;
            int h = s_hour % HOURS_PER_DAY;

            const app_settings_t *cfg = settings_get();
            float comfort_temp = cfg->control.limits.room_max;

            if(s_presence) {
                s_presence_true_seconds++;
                s_presence_false_seconds = 0;
                if(s_presence_true_seconds >= SHORT_PRESENCE_SEC && !s_presence_valid) {
                    s_presence_valid = true;
                    s_presence_start_time_sec = s_seconds_counter;
                }
            } else {
                s_presence_true_seconds = 0;
                if(s_presence_valid) {
                    s_presence_false_seconds++;
                    float bias_now = s_behavior_bias[wd][h];
                    int effective_timeout_min = s_presence_timeout_min;
                    if (bias_now > 0.75f) {
                        effective_timeout_min = (int)(s_presence_timeout_min * 1.5f);
                    }
                    if(s_presence_false_seconds >= (effective_timeout_min * SECONDS_PER_MINUTE)) {
                        s_presence_valid = false;
                        uint32_t duration_sec = s_seconds_counter - s_presence_start_time_sec;
                        if (duration_sec > (SHORT_PRESENCE_SEC + 60)) {
                            int dur_min = duration_sec / SECONDS_PER_MINUTE;
                            float new_to = (s_presence_timeout_min * 0.9f) + (dur_min * 0.05f);
                            s_presence_timeout_min = (int)clampf(new_to, MIN_PRESENCE_TIMEOUT, MAX_PRESENCE_TIMEOUT);
                        }
                    }
                }
            }

            s_preheat_active = false;
            if (!s_presence_valid && s_thermal_model.heat_rate_deg_per_h > 0.1f) {
                int current_total_min = (h * MINUTES_PER_HOUR) + s_minute;
                for (int i = 1; i <= PREHEAT_LOOKAHEAD_HOURS; ++i) {
                    int future_total_min = current_total_min + (i * MINUTES_PER_HOUR);
                    int future_hour = (future_total_min / MINUTES_PER_HOUR) % HOURS_PER_DAY;
                    int day_offset = future_total_min / (MINUTES_PER_HOUR * HOURS_PER_DAY);
                    int future_wd = (wd + day_offset) % DAYS_PER_WEEK;

                    if (s_behavior_bias[future_wd][future_hour] > PREHEAT_BIAS_THRESHOLD) {
                        float temp_gain_needed = comfort_temp - s_room_temp;
                        if (temp_gain_needed > 0.0f) {
                            float hours_to_heat = temp_gain_needed / s_thermal_model.heat_rate_deg_per_h;
                            int minutes_to_heat_early = (int)(hours_to_heat * MINUTES_PER_HOUR) + PREHEAT_BUFFER_MIN;

                            int minutes_until_target = (i * MINUTES_PER_HOUR);
                            if (minutes_until_target <= minutes_to_heat_early) {
                                s_preheat_active = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (s_seconds_counter != 0 && (s_seconds_counter % LEARNING_INTERVAL_SEC) == 0) {
                float current_bias = s_behavior_bias[wd][h];
                float new_bias = current_bias;
                if (s_presence_valid) {
                    new_bias += LEARNING_RATE_POSITIVE;
                } else {
                    new_bias -= LEARNING_RATE_NEGATIVE;
                }
                new_bias = clampf(new_bias, 0.0f, 1.0f);
                if (fabsf(new_bias - s_behavior_bias[wd][h]) > 0.001f) {
                    s_behavior_bias[wd][h] = new_bias;
                    s_bias_dirty = true;
                }
            }

            if (s_seconds_counter != 0 && (s_seconds_counter % NVS_SAVE_INTERVAL_SEC) == 0) {
                if (s_bias_dirty) {
                    if (nvs_save_blob_local(NVS_KEY_BIAS, s_behavior_bias, sizeof(s_behavior_bias)) == ESP_OK) s_bias_dirty = false;
                }
                if (s_model_dirty) {
                    if (nvs_save_blob_local(NVS_KEY_MODEL, &s_thermal_model, sizeof(s_thermal_model)) == ESP_OK) s_model_dirty = false;
                }
                if (s_setpoint_dirty) {
                    if (nvs_save_blob_local(NVS_KEY_SETPOINT, &s_current_setpoint, sizeof(s_current_setpoint)) == ESP_OK) s_setpoint_dirty = false;
                }
            }

            float target = compute_target_internal();
            float old_sp = s_current_setpoint;

            if(target > s_current_setpoint) {
                s_current_setpoint += fminf(SMOOTH_DELTA_PER_SEC, target - s_current_setpoint);
            }
            else if(target < s_current_setpoint) {
                s_current_setpoint -= fminf(SMOOTH_DELTA_PER_SEC, s_current_setpoint - target);
            }

            if (fabsf(old_sp - s_current_setpoint) > 0.01f) {
                s_setpoint_dirty = true;
            }

            xSemaphoreGive(s_lock);
        }
    }

    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t adaptive_thermo_init(void){
    s_lock = xSemaphoreCreateMutex();
    if(!s_lock) return ESP_ERR_NO_MEM;

    memset(s_behavior_bias, 0, sizeof(s_behavior_bias));
    nvs_load_blob_local(NVS_KEY_BIAS, s_behavior_bias, sizeof(s_behavior_bias));
    nvs_load_blob_local(NVS_KEY_MODEL, &s_thermal_model, sizeof(s_thermal_model));

    const app_settings_t *cfg = settings_get();
    s_current_setpoint = cfg->control.limits.room_min;

    nvs_load_blob_local(NVS_KEY_SETPOINT, &s_current_setpoint, sizeof(s_current_setpoint));

    s_task_running = true;
    BaseType_t r = xTaskCreatePinnedToCore(adaptive_calc_task, "at_task", 4096, NULL, 5, &s_task, tskNO_AFFINITY);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create adaptive_calc_task");
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Adaptive Algo Initialized. Last Setpoint: %.2f", s_current_setpoint);
    return ESP_OK;
}

void adaptive_thermo_notify_sensor(float room_temp, float outside_temp, bool presence, int weekday, int hour, int minute){
    if(!s_lock) {
        s_room_temp = room_temp;
        s_outside_temp = outside_temp;
        s_presence = presence;
        s_weekday = weekday;
        s_hour = hour;
        s_minute = minute;
        return;
    }
    if(xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_room_temp = room_temp;
        s_outside_temp = outside_temp;
        s_presence = presence;
        s_weekday = weekday;
        s_hour = hour;
        s_minute = minute;
        xSemaphoreGive(s_lock);
    } else {
        ESP_LOGW(TAG, "notify_sensor: lock busy, sensor update skipped");
    }
}

float adaptive_thermo_get_setpoint(void){
    float out = s_current_setpoint;
    if(!s_lock) return out;

    if(xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        out = s_current_setpoint;
        xSemaphoreGive(s_lock);
    }
    return out;
}

void adaptive_thermo_set_heat_rate(float degrees_per_hour) {
    if(!s_lock) return;
    if(xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (degrees_per_hour > 0.1f) {
            s_thermal_model.heat_rate_deg_per_h = degrees_per_hour;
            s_model_dirty = true;
        }
        xSemaphoreGive(s_lock);
    } else {
        ESP_LOGW(TAG, "set_heat_rate: lock busy");
    }
}

void adaptive_thermo_deinit(void){
    if(s_task) {
        s_task_running = false;
        for (int i=0; i<20 && s_task != NULL; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (s_task != NULL) {
            ESP_LOGW(TAG, "task did not exit in time, deleting forcefully");
            vTaskDelete(s_task);
            s_task = NULL;
        }
    }
    if(s_lock) {
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(2000)) == pdTRUE) {
            nvs_save_blob_local(NVS_KEY_BIAS, s_behavior_bias, sizeof(s_behavior_bias));
            nvs_save_blob_local(NVS_KEY_MODEL, &s_thermal_model, sizeof(s_thermal_model));
            nvs_save_blob_local(NVS_KEY_SETPOINT, &s_current_setpoint, sizeof(s_current_setpoint));
            xSemaphoreGive(s_lock);
        } else {
            ESP_LOGW(TAG, "deinit: failed to take lock for saving, skipping save to avoid deadlock");
        }
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
    }
    ESP_LOGI(TAG, "Adaptive Algo De-initialized.");
}