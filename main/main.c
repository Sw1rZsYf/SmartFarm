#include "main.h"
#include "esp_log.h"
#include "wifi_config.h"
#include "onenet_config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "onenet_config.h"
#include "app.h"
#include "gy30.h"
#include "sntp_time.h"
#include "lcd_ili9341.h"
#include "esp_log.h"
#include "tjc_driver.h"
#include "pwm_driver.h"

void sensor_task(void *arg);
void get_time_task(void *arg);
void Screen_init();
void tjc_init();
#define BIN_GPIO GPIO_NUM_5

static const char *MAIN_TAG = "Main";

sensor_data_t sensor_data = {
    .temperature = 0,
    .humidity = 0,
    .nh3_voltage = 0,
    .h2s_voltage = 0,
    .light = 0,
};

void sensor_task(void *arg);
// Wi-Fi连接成功后的回调函数

int pwm_enable = 0;
static void on_wifi_connected(void)
{
    ESP_LOGI(MAIN_TAG, "Wi-Fi connected, proceeding to start MQTT client.");
    // 启动MQTT客户端
    mqtt_onenet_start();
    initialize_sntp();                                                 // 初始化SNTP同步时间
    xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);     // 获取传感器数据并上报
    xTaskCreate(&get_time_task, "get_time_task", 4096, NULL, 5, NULL); // 获取实时时间
                                                                       // Screen_init();
    // 在初始化中创建任务
    // xTaskCreate(tjc_receive_task, "tjc_rx", 4096, NULL, 5, NULL);
    // sg90_set_angle_smooth(0, 180, 10, 20);

    switch_led(1);
    pwm_enable = 1;
}

void app_main(void)
{

    int number = 0;
    pwm_init(); // 初始化PWM系统
    pwm_set_duty(0, 0);
    pwm_start(0); // 启动通道0

    pwm_set_duty(2, 0); // 用于风扇
    pwm_start(2);       // 启动通道2

    adc_system_init(); // 初始化ADC系统
    gy30_init();       // 初始化光照传感器
    tjc_init();
    neopixel_init(LED_STRIP_PIN, LED_STRIP_LED_NUM); // 初始化RGB灯
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // 初始化Wi-Fi，并传入连接成功的回调函数
    ESP_LOGI(MAIN_TAG, "Initializing Wi-Fi...");
    wifi_connect_init(on_wifi_connected);

    // 等待传感器稳定
    vTaskDelay(pdMS_TO_TICKS(3000));
    QueueHandle_t queue = 0;

    queue = timerInitConfig(1000000, 2000000);

    switch_fan(1);
    int i = 0;
    while (1)
    {
        if (pwm_enable == 1)
        {
            // 从当前角度平滑转到90度，步进1度，每步延时20ms
            sg90_set_angle_smooth(0, 90, 5, 50);
            vTaskDelay(pdMS_TO_TICKS(2000));
            // 从当前角度平滑转到180度，步进2度，每步延时30ms
            sg90_set_angle_smooth(0, 180, 5, 50);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        if (xQueueReceive(queue, &number, pdMS_TO_TICKS(2000)))
        {
        }
    }
}

// 传感器采集任务
void sensor_task(void *arg)
{
    while (1)
    {
        // sim_read_sensors(&sensor_data.temperature, &sensor_data.humidity,
        //                  &sensor_data.nh3_voltage, &sensor_data.h2s_voltage,
        //                  &sensor_data.light);
        read_sensors(&sensor_data.temperature, &sensor_data.humidity,
                     &sensor_data.nh3_voltage, &sensor_data.h2s_voltage,
                     &sensor_data.light);
        int ret = report_sensor_data(sensor_data.temperature, sensor_data.humidity,
                                     sensor_data.nh3_voltage, sensor_data.h2s_voltage,
                                     sensor_data.light);

        tjc_sent_sensor(&sensor_data.temperature, &sensor_data.humidity,
                        &sensor_data.nh3_voltage, &sensor_data.h2s_voltage,
                        &sensor_data.light);
        // control(sensor_data.temperature, sensor_data.humidity,
        //         sensor_data.nh3_voltage, sensor_data.h2s_voltage,
        //         sensor_data.light);
        ESP_LOGI("SENSOR", "Temperature: %.2f C, Humidity: %.2f %%, NH3 Voltage: %d, H2S Voltage: %d, Light: %d",
                 sensor_data.temperature, sensor_data.humidity,
                 sensor_data.nh3_voltage, sensor_data.h2s_voltage, sensor_data.light);
        // 3. 等待10秒后再次上报

        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

    void get_time_task(void *arg)
{
    struct tm timeinfo;
    while (1)
    {
        calc_current_time();
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10s获取一次时间
    }
}
void Screen_init()
{
    // 初始化LCD，使用40MHz频率，0度旋转
    esp_err_t ret = lcd_init(40000000, LCD_ROTATION_0);
    if (ret != ESP_OK)
    {
        ESP_LOGE("MAIN", "LCD初始化失败");
        return;
    }

    ESP_LOGI("MAIN", "LCD初始化成功，开始演示...");
}

void tjc_init()
{
    ESP_LOGI("TJC", "系统启动，初始化淘晶驰串口屏...");

    // 1. 初始化串口
    esp_err_t ret = tjc_uart_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE("TJC", "串口初始化失败，重启中...");
        esp_restart();
    }
}

// void tjc_receive_task(void *pvParameters)
// {
//     char cmd_buffer[128];
//     while (1)
//     {
//         int len = tjc_receive_command(cmd_buffer, sizeof(cmd_buffer), 100); // 等待100ms
//         if (len > 0)
//         {
//             tjc_parse_command(cmd_buffer);
//         }
//         vTaskDelay(pdMS_TO_TICKS(50));
//     }
// }
