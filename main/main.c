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

void sensor_task(void *arg);
void get_time_task(void *arg);
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
static void on_wifi_connected(void)
{
    ESP_LOGI(MAIN_TAG, "Wi-Fi connected, proceeding to start MQTT client.");
    // 启动MQTT客户端
    mqtt_onenet_start();
    initialize_sntp(); // 初始化SNTP同步时间
    xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(&get_time_task, "get_time_task", 4096, NULL, 5, NULL);
}

void app_main(void)
{

    int number = 0;

    // adc_system_init(); // 初始化ADC系统
    // gy30_init();       // 初始化光照传感器

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES  || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // 3. 初始化Wi-Fi，并传入连接成功的回调函数
    ESP_LOGI(MAIN_TAG, "Initializing Wi-Fi...");
    wifi_connect_init(on_wifi_connected);

    neopixel_init(LED_STRIP_PIN, LED_STRIP_LED_NUM); //初始化RGB灯
    //等待传感器稳定
    vTaskDelay(pdMS_TO_TICKS(3000)); 
    QueueHandle_t queue = 0;
    // 初始化定时器 1秒进入回调函数一次
    queue = timerInitConfig(1000000,2000000);

    while (1)
    {
        //从队列中接收一个数据
        if (xQueueReceive(queue, &number, pdMS_TO_TICKS(2000)))
        {
            // RGB_Blink();            //正常运行指示+
        }
    }
    
}

//传感器采集任务
void sensor_task(void *arg) {
    while (1) {
        sim_read_sensors(&sensor_data.temperature, &sensor_data.humidity,
                        &sensor_data.nh3_voltage, &sensor_data.h2s_voltage,
                        &sensor_data.light);
        int ret = report_sensor_data(sensor_data.temperature, sensor_data.humidity,
                                        sensor_data.nh3_voltage, sensor_data.h2s_voltage,
                                        sensor_data.light);

        ESP_LOGI("SENSOR", "Temperature: %.2f C, Humidity: %.2f %%, NH3 Voltage: %d, H2S Voltage: %d, Light: %d",
                    sensor_data.temperature, sensor_data.humidity,
                    sensor_data.nh3_voltage, sensor_data.h2s_voltage, sensor_data.light);
        // 3. 等待10秒后再次上报

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}


void get_time_task(void *arg) {
    struct tm timeinfo;
    while (1) {
        calc_current_time();
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10s获取一次时间
    }
}

