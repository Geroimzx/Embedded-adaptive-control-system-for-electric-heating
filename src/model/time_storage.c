#include "time_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

static const char* TAG = "time_storage";
static const char* NVS_NAMESPACE = "storage";
static const char* NVS_KEY = "last_time";

esp_err_t time_storage_save_time(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    time_t current_time = time(NULL);
    err = nvs_set_i64(my_handle, NVS_KEY, (int64_t)current_time);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save time to NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Time saved to NVS: %lld", (long long)current_time);
    }
    nvs_close(my_handle);
    return err;
}

esp_err_t time_storage_restore_time(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error opening NVS handle. Time cannot be restored.");
        return err;
    }

    int64_t stored_time;
    err = nvs_get_i64(my_handle, NVS_KEY, &stored_time);
    nvs_close(my_handle);
    
    if (err == ESP_OK) {
        struct timeval tv = { .tv_sec = stored_time, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "Time restored from NVS: %lld", (long long)stored_time);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Failed to restore time from NVS. No time was set.");
        return err;
    }
}