#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"




// 定义通道
#define NH3_CHANNEL    ADC_CHANNEL_5    //ADC2-CH5
#define H2S_CHANNEL    ADC_CHANNEL_0    //ADC2-CH0
#define LIGHT_CHANNEL  ADC_CHANNEL_5    //ADC1-CH5
#define ADC_UNIT_LIGHT     ADC_UNIT_1
#define ADC_UNIT_GUS       ADC_UNIT_2
#define ADC_ATTEN      ADC_ATTEN_DB_12

// ADC 初始化函数
esp_err_t adc_init(adc_unit_t adc_unit, adc_channel_t channel, adc_atten_t atten);

// ADC 校准函数
adc_cali_handle_t adc_calibrate(adc_unit_t adc_unit, adc_atten_t atten);

// 读取 ADC 电压值
int adc_read_voltage(adc_unit_t adc_unit, adc_channel_t channel, adc_cali_handle_t handle);

// 获取ADC单元句柄（内部使用，也可用于高级应用）
adc_oneshot_unit_handle_t get_adc_unit_handle(adc_unit_t adc_unit);

// ADC 系统初始化（初始化所有需要的ADC通道和校准）
void adc_system_init(void);

// ADC 系统读取函数（读取NH3和H2S传感器的电压值）
void adc_system_read(int *nh3_voltage, int *h2s_voltage, int *light_voltage);

#endif // ADC_DRIVER_H