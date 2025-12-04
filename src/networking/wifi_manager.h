#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wifi_connected_cb_t)(void);

/**
 * @brief Ініціалізує Wi-Fi стек та обробники подій.
 * @return ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Запускає Wi-Fi менеджер у режимі SoftAP+STA.
 *
 * @param ap_password Пароль для SoftAP.
 * @param ap_ssid SSID для SoftAP.
 * @param sta_ssid SSID для клієнтського режиму STA.
 * @param sta_password Пароль для клієнтського режиму STA.
 * @param on_connected_cb Callback-функція, що викликається після успішного підключення.
 * @param on_disconnected_cb Callback-функція, що викликається після втрати підключення.
 * @return ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t wifi_manager_start(const char *ap_password, const char *ap_ssid,
                             const char *sta_ssid, const char *sta_password,
                             wifi_connected_cb_t on_connected_cb,
                             wifi_connected_cb_t on_disconnected_cb);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
