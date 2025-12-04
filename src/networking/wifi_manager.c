#include "networking/wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <string.h>

static const char *TAG = "wifi_manager";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static wifi_connected_cb_t s_on_connected_cb = NULL;
static wifi_connected_cb_t s_on_disconnected_cb = NULL;

static char s_ap_ssid[33];
static char s_ap_password[65];

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGW(TAG, "Wi-Fi STA: Disconnected");
        if (s_on_disconnected_cb) {
            s_on_disconnected_cb();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi STA: Connected with IP:" IPSTR, IP2STR(&event->ip_info.ip));
        if (s_on_connected_cb) {
            s_on_connected_cb();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "SoftAP started: SSID='%s', Password='%s'", s_ap_ssid, s_ap_password);
    }
}

esp_err_t wifi_manager_init(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    ESP_LOGI(TAG, "Wi-Fi manager initialized.");
    return ESP_OK;
}

esp_err_t wifi_manager_start(const char *ap_password, const char *ap_ssid,
                             const char *sta_ssid, const char *sta_password,
                             wifi_connected_cb_t on_connected_cb,
                             wifi_connected_cb_t on_disconnected_cb) {
    s_on_connected_cb = on_connected_cb;
    s_on_disconnected_cb = on_disconnected_cb;
    strcpy(s_ap_ssid, ap_ssid);
    strcpy(s_ap_password, ap_password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    
    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = strlen(ap_ssid),
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    strcpy((char*)ap_config.ap.ssid, ap_ssid);
    strcpy((char*)ap_config.ap.password, ap_password);
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    
    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    strcpy((char*)sta_config.sta.ssid, sta_ssid);
    strcpy((char*)sta_config.sta.password, sta_password);
    
    ESP_LOGI(TAG, "Attempting to connect to STA: SSID='%s'", sta_ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    return ESP_OK;
}
