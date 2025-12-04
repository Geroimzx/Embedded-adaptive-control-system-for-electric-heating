#include "controller/display/display_controller.h"
#include "view/display_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "networking/ntp_time_sync.h"
#include "model/system_state.h"
#include "model/main_control.h"
#include "controller/temp_setpoint_manager.h"
#include <time.h>
#include <string.h>
#include "esp_netif.h"

static const char *TAG = "DisplayController";
static TimerHandle_t display_update_timer;
sensors_state_t current_state;

static void display_update_timer_callback(TimerHandle_t xTimer) {
    system_state_get(&current_state);
    display_controller_update();
}

void display_controller_update(void) {
    switch (current_state.ui_state) {
        case UI_STATE_SPLASH_SCREEN:
            render_splash_screen();
            break;

        case UI_STATE_MAIN_SCREEN:
            main_screen_data_t data;

            data.temperature = current_state.temperature_c_sensor1;
            data.wifi_connected = current_state.wifi_connected;

            char date_str[11];
            char time_str[6];
            
            get_current_date_string(date_str, sizeof(date_str));
            get_current_time_string(time_str, sizeof(time_str));

            data.time_str = time_str;
            data.date_str = date_str;
            switch (current_state.system_state) {
                case STATE_OFF:
                    data.mode_str = "OFF";
                    break;
                case STATE_MANUAL:
                    data.mode_str = "MANUAL";
                    break;
                case STATE_PROGRAMMED:
                    data.mode_str = "PROG";
                    break;
                case STATE_ADAPTIVE:
                    data.mode_str = "ADPT";
                    break;
                case STATE_ANTI_FREEZE:
                    data.mode_str = "FROST";
                    break;
                case STATE_EMERGENCY:
                    data.mode_str = "EMERG";
                    break;
                case STATE_MODE_SELECT:
                    data.mode_str = "SEL";
                    break;
                default:
                    data.mode_str = "---";
                    break;
            }
            render_main_screen(&data);
            break;

        case UI_STATE_SELECT_MENU:
            render_mode_select_screen(main_control_get_preview_state());
            break;

        case UI_STATE_TEMPERATURE_SELECT:
            render_temperature_select_screen(temp_setpoint_manager_get());
            break;

        case UI_STATE_INFO:
        {
            char ip_str[16] = "Scanning...";
            bool got_ip = false;

            esp_netif_t *netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif_sta) {
                esp_netif_ip_info_t ip_info;
                esp_netif_get_ip_info(netif_sta, &ip_info);
                if (ip_info.ip.addr != 0) {
                    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
                    got_ip = true;
                }
            }

            if (!got_ip) {
                esp_netif_t *netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
                if (netif_ap) {
                    esp_netif_ip_info_t ip_info;
                    esp_netif_get_ip_info(netif_ap, &ip_info);
                    if (ip_info.ip.addr != 0) {
                        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
                    } else {
                        strcpy(ip_str, "192.168.4.1"); 
                    }
                } else {
                    strcpy(ip_str, "No WiFi");
                }
            }
            
            #include "model/settings_manager.h"
            const app_settings_t *cfg = settings_get();
            
            render_info_screen(cfg->wifi.ap_ssid, cfg->wifi.ap_pass, ip_str);
            break;
        }

        case UI_STATE_EMERGENCY:
            render_emergency_screen(current_state.error_code);
            break;

        default:
            break;
    }
}

esp_err_t display_controller_init(gpio_num_t sda_gpio, gpio_num_t scl_gpio) {
    display_manager_init(sda_gpio, scl_gpio);
    
    display_update_timer = xTimerCreate(
        "DisplayUpdateTimer",
        pdMS_TO_TICKS(50),
        pdTRUE,
        (void*)0,
        display_update_timer_callback
    );

    if (display_update_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create display update timer.");
        return ESP_FAIL;
    }
    
    xTimerStart(display_update_timer, 0);

    ESP_LOGI(TAG, "Display controller initialized.");
    return ESP_OK;
}