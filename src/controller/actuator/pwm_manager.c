#include "controller/actuator/pwm_manager.h"
#include "controller/actuator/relay_controller.h" 
#include "model/settings_manager.h"
#include "esp_log.h"
#include <time.h> 
#include <inttypes.h>
#include <math.h>

static const char *TAG = "pwm_manager";

static time_t s_cycle_start_time = 0;

void pwm_manager_update(float pid_output, float current_radiator_temp) {
    const app_settings_t *cfg = settings_get();

    float safe_max_temp = cfg->control.limits.rad_max;
    int pwm_cycle_duration = cfg->control.pwm_cycle_s;

    if (pwm_cycle_duration <= 0) {
        pwm_cycle_duration = 60;
    }

    if(safe_max_temp >= 70.0f) {
        safe_max_temp = 70.0f;
    }

    if (current_radiator_temp >= safe_max_temp) {
        relay_controller_set_heater_state(false);
        
        ESP_LOGW(TAG, "SAFETY CUTOFF! Rad temp %.1f C exceeds limit %.1f C. Heater OFF.", 
                 current_radiator_temp, safe_max_temp);
        
        return; 
    }

    time_t now = time(NULL);

    if (s_cycle_start_time == 0 || (now - s_cycle_start_time) >= pwm_cycle_duration) {
        s_cycle_start_time = now;
        ESP_LOGI(TAG, "Starting new PWM cycle of %d seconds.", pwm_cycle_duration);
    }
    
    if (pid_output < 0.0f) pid_output = 0.0f;
    if (pid_output > 100.0f) pid_output = 100.0f;

    uint32_t on_duration_sec = (uint32_t)((pid_output / 100.0f) * pwm_cycle_duration);

    bool should_be_on = false;
    if (on_duration_sec > 0) {
        time_t elapsed_in_cycle = now - s_cycle_start_time;
        if (elapsed_in_cycle < on_duration_sec) {
            should_be_on = true;
        }
    }
    
    relay_controller_set_heater_state(should_be_on);

    ESP_LOGD(TAG, "PID: %.1f%% | Rad: %.1fC / Max: %.1fC | OnTime: %" PRIu32 "s of %ds | State: %s",
             pid_output, current_radiator_temp, safe_max_temp, 
             on_duration_sec, pwm_cycle_duration, should_be_on ? "ON" : "OFF");
}

void pwm_manager_reset(void) {
    ESP_LOGI(TAG, "Resetting PWM manager. Forcing heater OFF.");
    relay_controller_set_heater_state(false);
    s_cycle_start_time = 0;
}