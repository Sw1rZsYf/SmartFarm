#include "app.h"
#include "dht.h"
#include "gy30.h"
#include "sntp_time.h"
#include "pwm_driver.h"
static const char *TAG = "APP_TASK";

#define LIGHT_THRESHOLD_LOW 80   // 光照低于此值（lux）时打开窗帘（增加光照）
#define LIGHT_THRESHOLD_HIGH 150 // 光照高于此值（lux）时关闭窗帘（减少光照）

// 初始化默认值
farm_task_t TaskFeed =
    {
        .hour = 15,
        .min = 0,
        .mode = Manual,
        .flag = 0

};
farm_task_t TaskEmssion =
    {
        .hour = 15,
        .min = 0,
        .mode = Manual,
        .flag = 0

};
bool CurtainOpen = false;

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
        TaskFeed.flag = 1;
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
        TaskEmssion.flag = 1;
    }
}

void check_AutoTask()
{
    uint8_t current_hour = get_current_hour();
    uint8_t current_min = get_current_minute();

    if (TaskFeed.mode == 1 && current_hour >= TaskFeed.hour && current_min >= TaskFeed.min && TaskFeed.flag == 1)
    {
        runFeedTask();
        TaskFeed.flag = 0; // 执行一次后重置标志，避免重复执行
    }

    if (TaskEmssion.mode == 1 && current_hour >= TaskEmssion.hour && current_min >= TaskEmssion.min && TaskEmssion.flag == 1)
    {
        runEmssionTask();
        TaskEmssion.flag = 0; // 执行一次后重置标志，避免重复执行
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
    // // 风扇控制（保持不变）
    // if (temperature > 30 || nh3_concentration > 200 || h2s_concentration > 200)
    // {
    //     switch_fan(1);
    // }
    // else
    // {
    //     switch_fan(0);
    // }

    // 窗帘滞回控制
    // 利用全局变量 CurtainOpen 记录当前窗帘状态（true=打开，false=关闭）
    if (light < LIGHT_THRESHOLD_LOW)
    {
        // 光照严重不足，需要打开窗帘让阳光进入
        if (!CurtainOpen)
        {
            Opencurtain();      // 执行打开动作（内部已包含平滑转动）
            CurtainOpen = true; // 更新状态
            ESP_LOGI(TAG, "光照过弱，打开窗帘增加光照");
        }
    }
    else if (light > LIGHT_THRESHOLD_HIGH)
    {
        // 光照过强，需要关闭窗帘遮阳
        if (CurtainOpen)
        {
            Closecurtain(); // 执行关闭动作
            CurtainOpen = false;
            ESP_LOGI(TAG, "光照过强，关闭窗帘减少光照");
        }
    }
    else
    {
        // 中间区域（LOW ≤ light ≤ HIGH）保持当前状态，不做任何操作
        // 这形成了滞回区间，有效防止在阈值附近频繁开关
    }
}
void Opencurtain()
{
    sg90_set_angle_smooth(0, 90, 5, 50);
}
void Closecurtain()
{
    sg90_set_angle_smooth(0, 180, 5, 50);
}