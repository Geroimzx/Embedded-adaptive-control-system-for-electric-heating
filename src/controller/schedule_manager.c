#include "schedule_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <time.h>
#include <string.h>

static const char *TAG = "SCHEDULE_MGR";

#define NVS_NAMESPACE "schedule"
#define NVS_KEY_SCHEDULE "week_sched"

static week_schedule_t g_current_schedule;

static void create_default_schedule(week_schedule_t* schedule) {
    ESP_LOGI(TAG, "Creating default schedule...");
    memset(schedule, 0, sizeof(week_schedule_t));

    day_schedule_t weekday_schedule = {
        .num_points = 4,
        .points = {
            { .hour = 6,  .minute = 30, .temperature = 21.0f },
            { .hour = 9,  .minute = 0,  .temperature = 18.0f },
            { .hour = 18, .minute = 0,  .temperature = 21.5f },
            { .hour = 23, .minute = 0,  .temperature = 22.0f }
        }
    };
    for (int i = 1; i <= 5; i++) {
        memcpy(&schedule->days[i], &weekday_schedule, sizeof(day_schedule_t));
    }

    day_schedule_t weekend_schedule = {
        .num_points = 2,
        .points = {
            { .hour = 8,  .minute = 0, .temperature = 21.0f },
            { .hour = 23, .minute = 30, .temperature = 22.0f }
        }
    };
    memcpy(&schedule->days[0], &weekend_schedule, sizeof(day_schedule_t));
    memcpy(&schedule->days[6], &weekend_schedule, sizeof(day_schedule_t));
}

esp_err_t schedule_manager_init(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(week_schedule_t);
    err = nvs_get_blob(nvs_handle, NVS_KEY_SCHEDULE, &g_current_schedule, &required_size);

    if (err == ESP_OK && required_size == sizeof(week_schedule_t)) {
        ESP_LOGI(TAG, "Successfully loaded schedule from NVS.");
    } else {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Schedule not found in NVS. Creating and saving default schedule.");
        } else {
            ESP_LOGE(TAG, "NVS read error (%s). Creating default schedule.", esp_err_to_name(err));
        }
        create_default_schedule(&g_current_schedule);
        err = nvs_set_blob(nvs_handle, NVS_KEY_SCHEDULE, &g_current_schedule, sizeof(week_schedule_t));
        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to commit default schedule to NVS!");
            }
        } else {
            ESP_LOGE(TAG, "Failed to write default schedule to NVS!");
        }
    }
    
    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t schedule_manager_save_schedule(const week_schedule_t* schedule) {
    memcpy(&g_current_schedule, schedule, sizeof(week_schedule_t));
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs_handle, NVS_KEY_SCHEDULE, &g_current_schedule, sizeof(week_schedule_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Schedule saved to NVS with result: %s", esp_err_to_name(err));
    return err;
}


float schedule_manager_get_current_setpoint(void) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int current_day_of_week = timeinfo.tm_wday;
    int yesterday = (current_day_of_week - 1 + DAYS_IN_WEEK) % DAYS_IN_WEEK;
    
    day_schedule_t* today_schedule = &g_current_schedule.days[current_day_of_week];
    day_schedule_t* yesterday_schedule = &g_current_schedule.days[yesterday];

    float setpoint = 18.0f;
    if (yesterday_schedule->num_points > 0) {
        setpoint = yesterday_schedule->points[yesterday_schedule->num_points - 1].temperature;
    }

    int current_time_in_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    
    for (int i = 0; i < today_schedule->num_points; i++) {
        schedule_point_t* point = &today_schedule->points[i];
        int point_time_in_minutes = point->hour * 60 + point->minute;
        
        if (current_time_in_minutes >= point_time_in_minutes) {
            setpoint = point->temperature;
        }
    }

    return setpoint;
}

void schedule_manager_get_full(week_schedule_t *out) {
    memcpy(out, &g_current_schedule, sizeof(week_schedule_t));
}