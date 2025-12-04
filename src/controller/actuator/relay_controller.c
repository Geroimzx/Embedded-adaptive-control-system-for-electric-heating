#include "controller/actuator/relay_controller.h"
#include "drivers/actuator/relay_driver.h"
#include "esp_log.h"

static const char *TAG = "relay_controller";

static bool s_heater_desired_state = false;

esp_err_t relay_controller_init(gpio_num_t heater_gpio_num, int heater_active_level) {
    ESP_LOGI(TAG, "Initializing relay controller...");
    esp_err_t err = relay_driver_init(heater_gpio_num, heater_active_level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize relay driver: %s", esp_err_to_name(err));
        return err;
    }

    s_heater_desired_state = false;
    relay_driver_set_state(s_heater_desired_state);

    ESP_LOGI(TAG, "Relay controller initialized. Heater initially OFF.");
    return ESP_OK;
}

esp_err_t relay_controller_set_heater_state(bool state) {
    if (s_heater_desired_state != state) {
        s_heater_desired_state = state;
        ESP_LOGI(TAG, "Setting heater state to: %s", state ? "ON" : "OFF");
        return relay_driver_set_state(state);
    }
    ESP_LOGD(TAG, "Heater state already %s. No change.", state ? "ON" : "OFF");
    return ESP_OK;
}

bool relay_controller_get_heater_state(void) {
    return s_heater_desired_state;
}