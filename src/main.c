#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include <time.h>
#include <math.h>

#include "hw_config.h"

#include "networking/ntp_time_sync.h"
#include "networking/wifi_manager.h"
#include "networking/weather_client.h"
#include "networking/web_server.h"

#include "model/settings_manager.h"
#include "model/time_storage.h"
#include "model/system_state.h"

#include "controller/input/button_controller.h"
#include "controller/display/display_controller.h"
#include "controller/sensor/temp_controller.h"
#include "controller/actuator/pwm_manager.h"
#include "controller/actuator/relay_controller.h"
#include "controller/sensor/presence_controller.h"
#include "controller/actuator/pid_controller.h" 
#include "controller/adaptive_algorythm.h"
#include "controller/temp_setpoint_manager.h"
#include "model/main_control.h"
#include "controller/schedule_manager.h"
#include "networking/mqtt_client.h"
#include "networking/mqtt_task.h"

static const char* TAG = "MAIN";

sensors_state_t current_sensors_state;

static system_state_t current_state = STATE_BOOT;
static SemaphoreHandle_t state_mutex;

static system_state_t preview_state = STATE_MANUAL;
static TickType_t mode_select_enter_time;
#define MODE_SELECT_TIMEOUT_MS 5000

static float prev_room_temp = -999.0f;
static float prev_rad_temp = -999.0f;
static TickType_t last_temp_check_time = 0;

typedef enum {
    ERR_NONE = 0,
    ERR_SENSOR_ROOM_FAIL,
    ERR_SENSOR_RAD_FAIL,
    ERR_SENSOR_SPIKE,
    ERR_RAD_OVERHEAT,
    ERR_WEATHER_CRITICAL
} system_error_t;

static system_error_t active_error = ERR_NONE;

const char* state_to_string(system_state_t state) {
    switch (state) {
        case STATE_OFF: return "OFF";
        case STATE_MANUAL: return "MANUAL";
        case STATE_PROGRAMMED: return "PROGRAMMED";
        case STATE_ADAPTIVE: return "ADAPTIVE";
        case STATE_ANTI_FREEZE: return "ANTI_FREEZE";
        case STATE_EMERGENCY: return "EMERGENCY";
        case STATE_MODE_SELECT: return "MODE SELECT";
        default: return "UNKNOWN";
    }
}

bool check_system_safety(float room_t, float rad_t, const app_settings_t *cfg, system_state_t current_mode) {
    
    if (room_t < SENSOR_MIN_VALID_TEMP || room_t > SENSOR_MAX_VALID_TEMP) {
        ESP_LOGE(TAG, "SAFETY: Room sensor failure! Val: %.2f", room_t);
        active_error = ERR_SENSOR_ROOM_FAIL;
        return false;
    }

    if (rad_t < SENSOR_MIN_VALID_TEMP || rad_t > SENSOR_MAX_VALID_TEMP) {
        ESP_LOGE(TAG, "SAFETY: Radiator sensor failure! Val: %.2f", rad_t);
        active_error = ERR_SENSOR_RAD_FAIL;
        return false;
    }

    if (rad_t >= cfg->control.limits.rad_max + 5.0f) {
        ESP_LOGE(TAG, "SAFETY: Radiator Overheat! %.2f > %.2f", rad_t, cfg->control.limits.rad_max + 5.0f);
        active_error = ERR_RAD_OVERHEAT;
        return false;
    }

    TickType_t now = xTaskGetTickCount();
    if (prev_room_temp > -900.0f) {
        float delta_time_sec = (float)(now - last_temp_check_time) / pdMS_TO_TICKS(1000);
        if (delta_time_sec > 0.5f) {
            float delta_room = fabsf(room_t - prev_room_temp);

            if (delta_room >= MAX_TEMP_JUMP_PER_SEC) {
                 ESP_LOGE(TAG, "SAFETY: Room Temp Spike! Delta: %.2f in %.1fs", delta_room, delta_time_sec);
                 active_error = ERR_SENSOR_SPIKE;
                 return false;
            }
        }
    }
    
    prev_room_temp = room_t;
    prev_rad_temp = rad_t;
    last_temp_check_time = now;
    
    bool weather_ok = weather_get_temperature() > -100.0f;

    if (current_mode == STATE_ADAPTIVE) {
        if (!weather_ok) {
            ESP_LOGE(TAG, "SAFETY: Critical Weather Data Unavailable!");
            active_error = ERR_WEATHER_CRITICAL;
            return false;
        }
    } else {
        if (!weather_ok) {
             ESP_LOGW(TAG, "Info: Weather unavailable, but not critical for current mode.");
        }
    }

    active_error = ERR_NONE;
    return true;
}

void main_control_change_state(system_state_t new_state) {
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        if (current_state != new_state) {
            pwm_manager_reset();
            ESP_LOGI(TAG, "STATE CHANGE: %s -> %s", state_to_string(current_state), state_to_string(new_state));
            current_state = new_state;
            
            if (new_state == STATE_MODE_SELECT) {
                preview_state = STATE_MANUAL;
                mode_select_enter_time = xTaskGetTickCount();
            }
        }
        xSemaphoreGive(state_mutex);
    }
}

system_state_t main_control_get_state(void) {
    system_state_t state = STATE_OFF; 
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        state = current_state;
        xSemaphoreGive(state_mutex);
    }
    return state;
}

void main_control_cycle_preview_mode(bool go_up) {
    if (main_control_get_state() != STATE_MODE_SELECT) return;

    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        system_state_t selectable_states[] = {STATE_MANUAL, STATE_ADAPTIVE, STATE_PROGRAMMED, STATE_ANTI_FREEZE};
        int num_states = sizeof(selectable_states) / sizeof(selectable_states[0]);
        int current_idx = -1;

        for (int i = 0; i < num_states; i++) {
            if (preview_state == selectable_states[i]) {
                current_idx = i;
                break;
            }
        }
        if (current_idx == -1) current_idx = 0;

        if (go_up) {
            current_idx = (current_idx + 1) % num_states;
        } else {
            current_idx = (current_idx - 1 + num_states) % num_states;
        }
        
        preview_state = selectable_states[current_idx];

        ESP_LOGI(TAG, "Preview mode changed to: %s", state_to_string(preview_state));
        mode_select_enter_time = xTaskGetTickCount();
        xSemaphoreGive(state_mutex);
    }
}

system_state_t main_control_get_preview_state(void) {
    system_state_t s = STATE_OFF;
    if (xSemaphoreTake(state_mutex, portMAX_DELAY) == pdTRUE) {
        s = preview_state;
        xSemaphoreGive(state_mutex);
    }
    return s;
}

int zeller_day_of_week(int d, int m, int y){
    if(m < 3){ m += 12; y -= 1; }
    int K = y % 100;
    int J = y / 100;
    int h = (d + 13*(m+1)/5 + K + K/4 + J/4 + 5*J) % 7;
    return (h + 6) % 7;
}

void heating_control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Heating control task started.");
    
    const app_settings_t *cfg = settings_get();

    main_control_change_state(STATE_OFF); 

    pid_controller_t heater_pid;
    pid_init(&heater_pid, 
             cfg->control.pid.kp, 
             cfg->control.pid.ki, 
             cfg->control.pid.kd, 
             0.0f, 100.0f);
    
    float room_temp, radiator_temp;
    bool presence_detected;
    bool heater_state;
    float outside_temp;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t loop_period = pdMS_TO_TICKS(1000);

    for (;;) {
        system_state_t active_state = main_control_get_state();
        system_state_set_system_state(active_state);
        system_state_set_temp_outside(weather_get_temperature());
        system_state_set_relay_state(relay_controller_get_heater_state());

        system_state_get(&current_sensors_state);
        room_temp = current_sensors_state.temperature_c_sensor1;
        radiator_temp = current_sensors_state.temperature_c_sensor2;
        presence_detected = current_sensors_state.presence_state;
        heater_state = current_sensors_state.relay_is_on;
        outside_temp = current_sensors_state.temperature_c_outside;

        if (!check_system_safety(room_temp, radiator_temp, cfg, active_state)) {
            if (active_state != STATE_EMERGENCY) {
                main_control_change_state(STATE_EMERGENCY);
            }
            
            relay_controller_set_heater_state(false); 
            pwm_manager_reset();

            system_state_set_error_code(active_error);
            system_state_set_ui_state(UI_STATE_EMERGENCY);
            ESP_LOGE(TAG, "SYSTEM IN EMERGENCY STATE! Error Code: %d", active_error);
            
            vTaskDelayUntil(&last_wake_time, loop_period);
            continue; 
        }
        
        ESP_LOGI(TAG, "Room: %.2f, Rad: %.2f, Out: %.2f, Pres: %d, Heat: %d", 
            room_temp, radiator_temp, outside_temp, presence_detected, heater_state);
        
        char date_str[11];
        char time_str[6];
        get_current_time_string(time_str, sizeof(time_str));
        get_current_date_string(date_str, sizeof(date_str));

        adaptive_thermo_notify_sensor(
            room_temp, outside_temp, presence_detected,
            zeller_day_of_week((date_str[0]-'0')*10 + (date_str[1]-'0'), (date_str[3]-'0')*10 + (date_str[4]-'0'), (date_str[6]-'0')*1000 + (date_str[7]-'0')*100 + (date_str[8]-'0')*10 + (date_str[9]-'0')),
            (time_str[0] - '0') * 10 + (time_str[1] - '0'), (time_str[3] - '0') * 10 + (time_str[4] - '0')
        );

        float pid_output_f = 0.0f;
        float setpoint_temp;

        switch (active_state) {
            case STATE_OFF:
                pwm_manager_reset();
                break;

            case STATE_MANUAL:
                setpoint_temp = temp_setpoint_manager_get();
                ESP_LOGI(TAG, "Manual setpoint: %.2fC", setpoint_temp);
                system_state_set_current_setpoint(setpoint_temp);
                pid_output_f = pid_compute(&heater_pid, setpoint_temp, room_temp);
                pwm_manager_update(pid_output_f, radiator_temp);
                break;

            case STATE_MODE_SELECT:
                if(current_sensors_state.ui_state != UI_STATE_SELECT_MENU) {
                    system_state_set_ui_state(UI_STATE_SELECT_MENU);
                }
                if ((xTaskGetTickCount() - mode_select_enter_time) > pdMS_TO_TICKS(MODE_SELECT_TIMEOUT_MS)) {
                    main_control_change_state(preview_state);
                    system_state_set_ui_state(UI_STATE_MAIN_SCREEN);
                }
                break;

            case STATE_PROGRAMMED:
                setpoint_temp = schedule_manager_get_current_setpoint();
                ESP_LOGI(TAG, "Programmed setpoint: %.2fC", setpoint_temp);
                system_state_set_current_setpoint(setpoint_temp);
                pid_output_f = pid_compute(&heater_pid, setpoint_temp, room_temp);
                pwm_manager_update(pid_output_f, radiator_temp);
                break;

            case STATE_ADAPTIVE:
                setpoint_temp = adaptive_thermo_get_setpoint();
                ESP_LOGI(TAG, "Adaptive setpoint: %.2fC", setpoint_temp);
                system_state_set_current_setpoint(setpoint_temp);
                pid_output_f = pid_compute(&heater_pid, setpoint_temp, room_temp);
                pwm_manager_update(pid_output_f, radiator_temp);
                break;

            case STATE_ANTI_FREEZE:
                if (room_temp <= cfg->control.limits.room_min) {
                    pid_output_f = pid_compute(&heater_pid, 7.0f, room_temp);
                    system_state_set_current_setpoint(7.0f);
                } else {
                    pid_output_f = 0;
                }
                pwm_manager_update(pid_output_f, radiator_temp);
                break;

            case STATE_EMERGENCY:
                relay_controller_set_heater_state(false);
                pwm_manager_reset();
                break;
            
            default:
                main_control_change_state(STATE_OFF);
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void on_wifi_connected_and_got_ip(void) {
    const app_settings_t *cfg = settings_get();

    system_state_set_wifi_connected(true);
    ESP_LOGI(TAG, "Wi-Fi ready. Init NTP...");
    ntp_time_sync_init();
    
    int retries = 0;
    while (!ntp_time_sync_is_synced() && retries < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retries++;
    }
    
    time_storage_save_time();

    mqtt_init(); 
    mqtt_publish_task_start();

    if (weather_init(cfg->geo.lat, cfg->geo.lon, cfg->geo.interval_min)) {
        ESP_LOGI(TAG, "Weather task started (Lat: %.2f, Lon: %.2f)", cfg->geo.lat, cfg->geo.lon);
    } else {
        ESP_LOGE(TAG, "Failed to start weather task.");
    }

    start_web_server();
}

esp_err_t thermostat_controller_init(void) {
    const app_settings_t *cfg = settings_get();

    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) return ESP_FAIL;

    system_state_init();
    ESP_ERROR_CHECK(display_controller_init(GPIO_SDA, GPIO_SCL));
    
    setenv("TZ", cfg->timezone, 1);
    tzset();
    
    button_controller_button_config_t button_configs[] = {
        { .gpio_num = GPIO_BUTTON_UP_PIN, .long_press_threshold_ms = 2000 },
        { .gpio_num = GPIO_BUTTON_DOWN_PIN, .long_press_threshold_ms = 2000 }
    };

    temp_controller_config_t temp_config = {
        .update_interval_ms = 500, .task_priority = 5, .task_stack_size = 4096
    };
    
    if (time_storage_restore_time() == ESP_OK) {
        time_t now = time(NULL);
        ESP_LOGI(TAG, "Time restored: %s", asctime(localtime(&now)));
    }
    
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(wifi_manager_start(
        cfg->wifi.ap_pass, 
        cfg->wifi.ap_ssid, 
        cfg->wifi.sta_ssid, 
        cfg->wifi.sta_pass, 
        on_wifi_connected_and_got_ip, 
        NULL
    ));

    ESP_ERROR_CHECK(button_controller_init(button_configs, sizeof(button_configs) / sizeof(button_configs[0])));
    ESP_ERROR_CHECK(presence_controller_init(HLK_PRESENCE_PIN));
    ESP_ERROR_CHECK(relay_controller_init(GPIO_RELAY, RELAY_ACTIVE_LEVEL));
    ESP_ERROR_CHECK(temp_controller_init(&temp_config));

    ESP_ERROR_CHECK(temp_setpoint_manager_init());
    ESP_ERROR_CHECK(schedule_manager_init());
    ESP_ERROR_CHECK(adaptive_thermo_init());

    system_state_set_ui_state(UI_STATE_MAIN_SCREEN);
    return ESP_OK;
}

// --- Головна функція ---

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(settings_init());
    
    ESP_ERROR_CHECK(thermostat_controller_init());
    xTaskCreate(heating_control_task, "Heating Control Task", 4096, NULL, 5, NULL);
}