#include "drivers/sensor/temp_sensor_driver.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "temp_sensor_drv";

#define KELVIN_OFFSET 273.15f

typedef struct ntc_sensor_handle_t {
    adc_channel_t channel;
    adc_cali_handle_t cali_handle;
    ntc_thermistor_config_t config;
    bool is_initialized;
} ntc_sensor_t;

static adc_oneshot_unit_handle_t s_adc1_handle;
static ntc_sensor_t s_ntc_sensors[MAX_NTC_SENSORS];
static int s_num_initialized_sensors = 0;

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    ESP_LOGI(TAG, "Attempting calibration with Line Fitting scheme");
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
        calibrated = true;
        ESP_LOGI(TAG, "Calibration successful");
    } else {
        ESP_LOGE(TAG, "Line Fitting calibration failed!");
    }
    *out_handle = handle;
    return calibrated;
}

ntc_sensor_handle_t temp_sensor_driver_init_ntc(adc_channel_t adc_channel,
                                                adc_atten_t adc_attenuation,
                                                adc_bitwidth_t adc_width_bit,
                                                const ntc_thermistor_config_t *ntc_config) {
    if (s_num_initialized_sensors >= MAX_NTC_SENSORS) {
        ESP_LOGE(TAG, "Maximum number of NTC sensors reached.");
        return NULL;
    }

    if (s_adc1_handle == NULL) {
        ESP_LOGI(TAG, "Initializing ADC1 unit...");
        adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &s_adc1_handle));
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = adc_width_bit,
        .atten = adc_attenuation,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, adc_channel, &config));

    ntc_sensor_t *sensor = &s_ntc_sensors[s_num_initialized_sensors];
    memset(sensor, 0, sizeof(ntc_sensor_t));

    sensor->channel = adc_channel;
    if (!adc_calibration_init(ADC_UNIT_1, adc_attenuation, &sensor->cali_handle)) {
        ESP_LOGE(TAG, "ADC calibration failed for sensor on channel %d", adc_channel);
        return NULL;
    }

    sensor->config = *ntc_config;
    sensor->is_initialized = true;
    s_num_initialized_sensors++;

    ESP_LOGI(TAG, "NTC Temp Sensor %d initialized (Channel: %d)", s_num_initialized_sensors, sensor->channel);
    ESP_LOGI(TAG, "NTC Config: R0=%.1f, T0=%.1fC, B=%.1f, R_Fixed=%.1f",
             sensor->config.nominal_resistance, sensor->config.nominal_temperature_c,
             sensor->config.b_value, sensor->config.fixed_resistor_ohms);

    return (ntc_sensor_handle_t)sensor;
}

esp_err_t temp_sensor_driver_read_ntc(ntc_sensor_handle_t handle, float *temperature_c) {
    if (handle == NULL || temperature_c == NULL) return ESP_ERR_INVALID_ARG;
    ntc_sensor_t *sensor = (ntc_sensor_t *)handle;
    if (!sensor->is_initialized) return ESP_ERR_INVALID_STATE;

    int adc_raw, voltage_mv;
    ESP_ERROR_CHECK(adc_oneshot_read(s_adc1_handle, sensor->channel, &adc_raw));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor->cali_handle, adc_raw, &voltage_mv));
    float voltage = (float)voltage_mv / 1000.0f;
    float r_ntc;

    if (voltage > 0.001f && voltage < 3.3f) {
        r_ntc = sensor->config.fixed_resistor_ohms * ((3.3f / voltage) - 1.0f);
    } else {
        *temperature_c = NAN;
        return ESP_ERR_INVALID_STATE;
    }

    float t_kelvin = 1.0f / ((1.0f / (sensor->config.nominal_temperature_c + KELVIN_OFFSET)) +
                              (1.0f / sensor->config.b_value) * logf(r_ntc / sensor->config.nominal_resistance));

    *temperature_c = t_kelvin - KELVIN_OFFSET;
    
    return ESP_OK;
}