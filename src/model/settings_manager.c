#include "settings_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SETTINGS";
static const char *NVS_NAMESPACE = "config";
static const char *NVS_KEY = "main_cfg";

#define SETTINGS_MAGIC 0xA1B2C301 

static app_settings_t current_settings;

static void load_defaults(void) {
    ESP_LOGW(TAG, "Loading default settings...");
    memset(&current_settings, 0, sizeof(app_settings_t));

    // WiFi
    strcpy(current_settings.wifi.sta_ssid, "DESKTOP-RT2N4K0 9718");
    strcpy(current_settings.wifi.sta_pass, "86104q[P");
    strcpy(current_settings.wifi.ap_ssid, "Thermostat_AP");
    strcpy(current_settings.wifi.ap_pass, "password123");

    // MQTT
    strcpy(current_settings.mqtt.host, "demo.thingsboard.io");
    current_settings.mqtt.port = 1883;
    strcpy(current_settings.mqtt.token, "4aDZN8VTakk0L6iilnr5");

    // Geo (Kyiv)
    current_settings.geo.lat = 50.45f;
    current_settings.geo.lon = 30.52f;
    current_settings.geo.interval_min = 15;

    // Control
    current_settings.control.pid.kp = 10.0f;
    current_settings.control.pid.ki = 0.1f;
    current_settings.control.pid.kd = 0.5f;

    current_settings.control.limits.rad_max = 60.0f;
    current_settings.control.limits.room_min = 18.0f;
    current_settings.control.limits.room_max = 22.0f;
    current_settings.control.pwm_cycle_s = 60;

    // Timezone
    strcpy(current_settings.timezone, "EET-2EEST-3,M3.5.0/3,M10.5.0/4");

    // Magic ID
    current_settings.magic_id = SETTINGS_MAGIC;
}

esp_err_t settings_save(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY, &current_settings, sizeof(app_settings_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        ESP_LOGI(TAG, "Settings saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save blob: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
    return err;
}

esp_err_t settings_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    
    nvs_handle_t handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    
    bool load_def = false;

    if (err == ESP_OK) {
        size_t required_size = sizeof(app_settings_t);
        err = nvs_get_blob(handle, NVS_KEY, &current_settings, &required_size);
        
        if (err == ESP_OK) {
            if (required_size != sizeof(app_settings_t) || current_settings.magic_id != SETTINGS_MAGIC) {
                ESP_LOGW(TAG, "Settings structure mismatch or old version. Resetting to defaults.");
                load_def = true;
            } else {
                ESP_LOGI(TAG, "Settings loaded from NVS.");
            }
        } else {
            ESP_LOGW(TAG, "Settings not found in NVS.");
            load_def = true;
        }
        nvs_close(handle);
    } else {
        ESP_LOGW(TAG, "NVS namespace not found, creating new.");
        load_def = true;
    }

    if (load_def) {
        load_defaults();
        return settings_save();
    }

    return ESP_OK;
}

const app_settings_t* settings_get(void) {
    return &current_settings;
}

app_settings_t* settings_get_writeable(void) {
    return &current_settings;
}

esp_err_t settings_factory_reset(void) {
    load_defaults();
    return settings_save();
}