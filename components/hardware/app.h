#ifndef _APP_H_
#define _APP_H_

#include <stdio.h>
#include <stdint.h>
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

#define Light_PIN GPIO_NUM_6
#define NH3_PIN GPIO_NUM_16
#define H2S_PIN GPIO_NUM_11
#define DHT_GPIO_PIN GPIO_NUM_1

typedef struct
{
    float temperature;
    float humidity;
    float nh3_ppm;
    float h2s_ppm;
    int light;
} sensor_data_t;

typedef struct
{
    int hour;
    int min;
    enum
    {
        Auto = 0,
        Manual
    } mode;
    int flag;
    /* data */
} farm_task_t;

void read_sensors(float *temperature, float *humidity,
                  float *nh3_ppm, float *h2s_ppm, int *light);
void sim_read_sensors(float *temperature, float *humidity,
                      float *nh3_ppm, float *h2s_ppm, int *light);
void control(float temperature, float humidity,
             float nh3_concentration, float h2s_concentration, int light);
void setFeedTask(int hour, int min, int mode);
void runFeedTask();
void runClearTask(void);
void runOnLedTask(void);
void runOffLedTask(void);
void setLightTask(bool on);
void setHeatTask(bool on);
void setCurtainTask(bool open);
void setLightSystemMode(bool auto_mode);
bool isLightSystemAutoMode(void);
void setAutoIntervalsSec(uint32_t feed_interval_sec, uint32_t clear_interval_sec);
uint32_t getAutoFeedIntervalMs(void);
uint32_t getAutoClearIntervalMs(void);
void registerAutoFeedTaskHandle(TaskHandle_t task_handle);
void registerAutoClearTaskHandle(TaskHandle_t task_handle);
void runEmssionTask();
void setEmssionTask(int hour, int min, int mode);
void check_AutoTask();
void Opencurtain();
void Closecurtain();
#endif
