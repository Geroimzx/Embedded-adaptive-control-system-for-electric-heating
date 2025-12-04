#include "drivers/actuator/relay_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "relay_driver";
static gpio_num_t s_relay_gpio_num;
static int s_relay_active_level;

esp_err_t relay_driver_init(gpio_num_t gpio_num, int active_level) {
    s_relay_gpio_num = gpio_num;
    s_relay_active_level = active_level;

    gpio_reset_pin(s_relay_gpio_num);

    esp_err_t err = gpio_set_direction(s_relay_gpio_num, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO direction for relay: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_level(s_relay_gpio_num, !s_relay_active_level);

    ESP_LOGI(TAG, "Relay driver initialized on GPIO %d (active level: %d)", s_relay_gpio_num, s_relay_active_level);
    return ESP_OK;
}

esp_err_t relay_driver_set_state(bool state) {
    int level_to_set;

    if (state) {
        level_to_set = s_relay_active_level;
    } else {
        level_to_set = !s_relay_active_level;
    }

    esp_err_t err = gpio_set_level(s_relay_gpio_num, level_to_set);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set relay level: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Relay state set to %s (GPIO level: %d)", state ? "ON" : "OFF", level_to_set);
    return ESP_OK;
}

bool relay_driver_get_state(void) {
    return (gpio_get_level(s_relay_gpio_num) == s_relay_active_level);
}