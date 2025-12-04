#include "ntp_time_sync.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <time.h>
#include <string.h>

static const char *TAG = "ntp_time_sync";
static bool s_time_synced = false;


static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "NTP time synchronized from NTP server.");
    s_time_synced = true;
}

void ntp_time_sync_init(void) {
    s_time_synced = false;
    ESP_LOGI(TAG, "Initializing NTP time synchronization.");

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "ua.pool.ntp.org");
    esp_sntp_setservername(2, "time.google.com");
    
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_interval(30 * 60 * 1000);
    
    esp_sntp_init();
    
    ESP_LOGI(TAG, "NTP started with multiple servers and immediate sync mode.");
}

bool ntp_time_sync_is_synced(void) {
    return s_time_synced;
}

void get_current_date_string(char* date_str, size_t len) {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(date_str, len, "%d/%m/%Y", &timeinfo);
}

void get_current_time_string(char* time_str, size_t len) {
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(time_str, len, "%H:%M", &timeinfo);
}
