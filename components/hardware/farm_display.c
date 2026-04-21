/*
 * farm_display.c - 农场显示层实现（TJC 串口屏适配）
 */
#include <stdio.h>
#include "esp_log.h"
#include "tjc_driver.h"
#include "farm_display.h"

static const char *TAG = "FARM_DISPLAY";

void farm_display_init(void)
{
    if (tjc_uart_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "串口屏初始化失败");
    }
}

void farm_display_show_boot(const char *line1, const char *line2)
{
    if (line1)
    {
        tjc_printf("t0.txt=\"%s\"", line1);
    }
    if (line2)
    {
        tjc_printf("t1.txt=\"%s\"", line2);
    }
}

void farm_display_show_status(const char *wifi_state, const char *cloud_state, const char *device_state)
{
    if (wifi_state)
    {
        tjc_printf("t8.txt=\"%s\"", wifi_state);
        // tjc_printf("t15.txt=\"%s\"", cloud_state);
    }
    // if (cloud_state)
    // {
    //     tjc_printf("t9.txt=\"%s\"", cloud_state);
    // }
    // if (device_state)
    // {
    //     tjc_printf("t10.txt=\"%s\"", device_state);
    // }
}

void farm_display_show_sensor(const sensor_data_t *sensor_data)
{
    if (!sensor_data)
    {
        return;
    }

    float temperature = sensor_data->temperature;
    float humidity = sensor_data->humidity;
    float nh3_ppm = sensor_data->nh3_ppm;
    float h2s_ppm = sensor_data->h2s_ppm;
    int light = sensor_data->light;

    tjc_sent_sensor(&temperature, &humidity, &nh3_ppm, &h2s_ppm, &light);
}

void farm_display_show_time(const char *time_str)
{
    if (!time_str)
    {
        return;
    }

    // ESP_LOGI(TAG, "更新时间显示: %s", time_str);
    tjc_printf("t15.txt=\"%s\"", time_str);
}