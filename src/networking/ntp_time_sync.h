#ifndef NTP_TIME_SYNC_H
#define NTP_TIME_SYNC_H

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ініціалізує NTP-синхронізацію часу.
 */
void ntp_time_sync_init(void);

/**
 * @brief Перевіряє, чи було синхронізовано час.
 * @return true, якщо час синхронізовано, інакше false.
 */
bool ntp_time_sync_is_synced(void);

/**
 * @brief Отримує поточну дату у вигляді форматованого рядка.
 *
 * @param date_str Буфер для зберігання рядка дати (напр., "2025-08-01").
 * @param len Довжина буфера.
 */
void get_current_date_string(char* date_str, size_t len);

/**
 * @brief Отримує поточний час у вигляді форматованого рядка.
 *
 * @param time_str Буфер для зберігання рядка часу (напр., "18:00:00").
 * @param len Довжина буфера.
 */
void get_current_time_string(char* time_str, size_t len);

#ifdef __cplusplus
}
#endif

#endif
