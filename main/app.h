#ifndef _APP_H_
#define _APP_H_

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "hal/gpio_types.h"
#include "gpio_driver.h"
#include "timer.h"
#include "adc.h"
#include "dht.h"
#include "gpio_driver.h"

#define Light_PIN    GPIO_NUM_6
#define NH3_PIN      GPIO_NUM_16
#define H2S_PIN      GPIO_NUM_11
#define DHT_GPIO_PIN GPIO_NUM_1


typedef struct 
{
    float temperature;
    float humidity;
    int nh3_voltage;
    int h2s_voltage;
    int light;
}sensor_data_t;



void read_sensors(float *temperature, float *humidity,
                  int *nh3_voltage, int *h2s_voltage,int *light);
void sim_read_sensors(float *temperature, float *humidity,
                      int *nh3_voltage, int *h2s_voltage,int *light);

#endif
