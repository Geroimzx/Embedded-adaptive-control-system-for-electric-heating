#ifndef TIME_STORAGE_H
#define TIME_STORAGE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Зберігає поточний системний час у NVS.
 *
 * @return esp_err_t ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t time_storage_save_time(void);

/**
 * @brief Відновлює час із NVS і встановлює його як системний.
 *
 * @return esp_err_t ESP_OK у разі успіху, інакше код помилки.
 */
esp_err_t time_storage_restore_time(void);

#ifdef __cplusplus
}
#endif

#endif