#include "controller/actuator/pid_controller.h"
#include "esp_timer.h"
#include <math.h>

void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float out_min, float out_max)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->out_min = out_min;
    pid->out_max = out_max;
    
    pid->_integral = 0.0f;
    pid->_pre_error = 0.0f;
    pid->_last_time_us = esp_timer_get_time();
}

float pid_compute(pid_controller_t *pid, float setpoint, float measured_value)
{
    int64_t now_us = esp_timer_get_time();
    float dt = (float)(now_us - pid->_last_time_us) / 1000000.0f;
    pid->_last_time_us = now_us;

    if (dt <= 0.0f || dt > 100.0f) {
        dt = 1.0f;
    }
    
    float error = setpoint - measured_value;

    float p_out = pid->kp * error;

    pid->_integral += error * dt;
    
    if (pid->_integral > pid->out_max) {
        pid->_integral = pid->out_max;
    } else if (pid->_integral < pid->out_min) {
        pid->_integral = pid->out_min;
    }
    
    float i_out = pid->ki * pid->_integral;

    float derivative = (error - pid->_pre_error) / dt;
    float d_out = pid->kd * derivative;

    float output = p_out + i_out + d_out;

    if (output > pid->out_max) {
        output = pid->out_max;
    } else if (output < pid->out_min) {
        output = pid->out_min;
    }

    pid->_pre_error = error;

    return output;
}