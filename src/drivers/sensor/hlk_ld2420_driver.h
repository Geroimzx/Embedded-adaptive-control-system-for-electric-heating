#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Колбек при зміні GPIO-стану присутності (якщо підключено окремий пін) */
typedef void (*hlk_ld2420_handler_t)(bool present_state);

/**
 * @brief Ініціалізація GPIO-піну присутності
 * @param gpio_num  номер GPIO, до якого підключений вихід PRESENCE
 * @param handler   колбек, який викликається при зміні стану
 */
esp_err_t hlk_ld2420_driver_init(gpio_num_t gpio_num, hlk_ld2420_handler_t handler);

/**
 * @brief Поточний стан присутності з GPIO-піну (якщо використовується)
 */
bool hlk_ld2420_is_present(void);

#ifdef __cplusplus
}
#endif
