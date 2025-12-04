#ifndef COMPONENTS_DRIVERS_TEMP_SENSOR_DRIVER_H_
#define COMPONENTS_DRIVERS_TEMP_SENSOR_DRIVER_H_

#include "esp_err.h"
#include <stdbool.h>
#include "hal/adc_types.h"

#define MAX_NTC_SENSORS 2

typedef struct {
    float nominal_resistance;
    float nominal_temperature_c;
    float b_value;
    float fixed_resistor_ohms;
} ntc_thermistor_config_t;

typedef struct ntc_sensor_handle_t *ntc_sensor_handle_t;

/**
 * @brief Ініціалізує драйвер датчика температури на основі NTC і ADC1.
 *
 * @param adc_channel ADC канал (наприклад, ADC_CHANNEL_6 для GPIO34).
 * @param adc_attenuation Аттенюація ADC (наприклад, ADC_ATTEN_DB_12).
 * @param adc_width_bit Ширина бітів ADC (наприклад, ADC_BITWIDTH_12).
 * @param ntc_config Структура з параметрами NTC термістора та опорного резистора.
 * @return ntc_sensor_handle_t Дескриптор ініціалізованого датчика, або NULL у разі помилки.
 */
ntc_sensor_handle_t temp_sensor_driver_init_ntc(adc_channel_t adc_channel,
                                                adc_atten_t adc_attenuation,
                                                adc_bitwidth_t adc_width_bit, // <<< ВИПРАВЛЕНО ТИП
                                                const ntc_thermistor_config_t *ntc_config);

esp_err_t temp_sensor_driver_read_ntc(ntc_sensor_handle_t handle, float *temperature_c);

#endif /* COMPONENTS_DRIVERS_TEMP_SENSOR_DRIVER_H_ */