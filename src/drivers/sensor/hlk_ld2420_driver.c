#include "hlk_ld2420_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>
#include "hw_config.h"

static const char *TAG = "HLK_LD2420_UART";

static hlk_ld2420_handler_t g_presence_callback = NULL;
static volatile bool g_presence_state = false;

static void IRAM_ATTR hlk_gpio_isr_handler(void *arg)
{
    g_presence_state = (bool)gpio_get_level((gpio_num_t)arg);
    if (g_presence_callback) {
        g_presence_callback(g_presence_state);
    }
}

esp_err_t hlk_ld2420_driver_init(gpio_num_t gpio_num, hlk_ld2420_handler_t handler)
{
    g_presence_callback = handler;
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE};
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
        return ret;

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        return ret;

    ret = gpio_isr_handler_add(gpio_num, hlk_gpio_isr_handler, (void *)gpio_num);
    if (ret != ESP_OK)
        return ret;

    g_presence_state = gpio_get_level(gpio_num);
    ESP_LOGI(TAG, "Presence GPIO init OK, pin=%d", gpio_num);
    return ESP_OK;
}

bool hlk_ld2420_is_present(void)
{
    return g_presence_state;
}
