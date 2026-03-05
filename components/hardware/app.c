#include "app.h"
#include "dht.h"
#include "gy30.h"
#include "sntp_time.h"
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

void setEmssionTask(int hour, int min, int mode)
{
    if (mode == 0)
    {
        TaskEmssion.mode = 0;
        runEmssionTask();
    }
    else if (mode == 1)
    {
        TaskEmssion.hour = hour;
        TaskEmssion.min = min;
        TaskEmssion.mode = mode;
        ESP_LOGI(TAG, "设置自动排放时间为：%d点%d分", hour, min);
    }
}

void check_AutoTask()
{
    uint8_t current_hour = get_current_hour();
    uint8_t current_min = get_current_minute();

    if (TaskFeed.mode == 1 && current_hour >= TaskFeed.hour && current_min >= TaskFeed.min)
    {
        runFeedTask();
    }

    if (TaskEmssion.mode == 1 && current_hour >= TaskEmssion.hour && current_min >= TaskEmssion.min)
    {
        runEmssionTask();
    }
}
// 排粪
void runEmssionTask()
{
    ESP_LOGI(TAG, "开始排放！");
}
// 喂食
void runFeedTask()
{
    ESP_LOGI(TAG, "开始投料！");
}

void runOnLedTask(void)
{
    switch_led(1);
    ESP_LOGI(TAG, "执行命令: 开灯");
}

void runOffLedTask(void)
{
    switch_led(0);
    ESP_LOGI(TAG, "执行命令: 关灯");
}

static void heat(int On)
{
    if (On)
    {
        ESP_LOGI(TAG, "温度过低，开始加热");
    }
    else
    {
        ESP_LOGI(TAG, "加热关闭");
    }
}

void control(float temperature, float humidity,
             int nh3_concentration, int h2s_concentration, int light)
{

    if (temperature > 30 || nh3_concentration > 200 || h2s_concentration > 200)
    {
        switch_fan(1);
    }
    else
    {
        switch_fan(0);
    }

    if (temperature < 15)
    {
        heat(1);
    }
    else
    {
        heat(0);
    }

    if (light < 100)
    {
        switch_led(1);
    }
    else
    {
        switch_led(0);
    }
}
