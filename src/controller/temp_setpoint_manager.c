#include "temp_setpoint_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "TEMP_SETPOINT_MGR";

#define NVS_NAMESPACE "settings"
#define NVS_KEY_SETPOINT "setpoint_temp"

#define DEFAULT_SETPOINT_TEMP 21.0f

static float g_setpoint_temp = DEFAULT_SETPOINT_TEMP;

esp_err_t temp_setpoint_manager_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t my_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка (%s) відкриття NVS!", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(float);
    err = nvs_get_blob(my_handle, NVS_KEY_SETPOINT, &g_setpoint_temp, &required_size);

    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Успішно завантажено задану температуру з NVS: %.2f°C", g_setpoint_temp);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGW(TAG, "Температура в NVS не знайдена. Встановлено значення за замовчуванням: %.2f°C", DEFAULT_SETPOINT_TEMP);
            g_setpoint_temp = DEFAULT_SETPOINT_TEMP;
            esp_err_t write_err = nvs_set_blob(my_handle, NVS_KEY_SETPOINT, &g_setpoint_temp, sizeof(float));
            if(write_err == ESP_OK) {
                nvs_commit(my_handle);
            }
            break;
        default:
            ESP_LOGE(TAG, "Помилка (%s) читання з NVS!", esp_err_to_name(err));
    }

    nvs_close(my_handle);
    return ESP_OK;
}

void temp_setpoint_manager_set(float new_temp) {
    g_setpoint_temp = new_temp;
    ESP_LOGI(TAG, "Встановлено нову температуру: %.2f°C. Зберігаємо в NVS...", g_setpoint_temp);

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка (%s) відкриття NVS для запису!", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(my_handle, NVS_KEY_SETPOINT, &g_setpoint_temp, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка (%s) запису в NVS!", esp_err_to_name(err));
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка (%s) підтвердження запису в NVS!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Нова температура успішно збережена в NVS.");
    }
    
    nvs_close(my_handle);
}

float temp_setpoint_manager_get(void) {
    return g_setpoint_temp;
}