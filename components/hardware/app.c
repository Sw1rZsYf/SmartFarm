#include "app.h"
#include "dht.h"
#include "gy30.h"

static const char *TAG = "APP_TASK";

// 初始化默认值
farm_task_t TaskFeed =
    {
        .hour = 15,
        .min = 0,
        .mode = Manual

};
farm_task_t TaskEmssion =
    {
        .hour = 15,
        .min = 0,
        .mode = Manual

};

void read_sensors(float *temperature, float *humidity,
                  int *nh3_voltage, int *h2s_voltage, int *light)
{

    dht_read_float_data(DHT_TYPE_DHT11, DHT_GPIO_PIN, humidity, temperature);

    adc_system_read(nh3_voltage, h2s_voltage, light);

    *light = (int)gy30_read_light();
}

void sim_read_sensors(float *temperature, float *humidity,
                      int *nh3_voltage, int *h2s_voltage, int *light)
{
    // 模拟传感器数据
    *temperature = 25.0 + (rand() % 100) / 10.0; // 25.0 到 35.0 度
    *humidity = 40.0 + (rand() % 600) / 10.0;    // 40.0% 到 100.0%
    *nh3_voltage = 200 + (rand() % 800);         // 200 到 1000 mV
    *h2s_voltage = 150 + (rand() % 850);         // 150 到 1000 mV
    *light = 100 + (rand() % 900);               // 100 到 1000 lx
}

void setFeedTask(int hour, int min, int mode)
{
    if (mode == 0)
    {
        TaskFeed.mode = 0;
        runFeedTask();
    }
    else if (mode == 1)
    {
        TaskFeed.hour = hour;
        TaskFeed.min = min;
        TaskFeed.mode = mode;
        ESP_LOGI(TAG, "设置自动投料时间为：%d点%d分", hour, min);
    }
}

void runFeedTask()
{
    ESP_LOGI(TAG, "开始投料！");
}