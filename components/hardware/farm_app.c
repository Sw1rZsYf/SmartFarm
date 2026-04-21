/*
 * farm_app.c - 农场应用编排实现
 */
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi_config.h"
#include "onenet_config.h"
#include "sntp_time.h"
#include "adc.h"
#include "gy30.h"
#include "gpio_driver.h"
#include "pwm_driver.h"
#include "app.h"
#include "farm_app.h"
#include "farm_display.h"
#include "service_router.h"

static const char *TAG = "FARM_APP";

static bool s_wifi_connected = false;

static void wait_for_interval_update(TaskHandle_t task_handle, uint32_t (*interval_getter)(void))
{
    while (1)
    {
        uint32_t delay_ms = interval_getter();
        if (delay_ms == 0)
        {
            delay_ms = 1000;
        }

        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(delay_ms)) == 0)
        {
            break;
        }
    }

    (void)task_handle;
}

static void on_wifi_connected(void)
{
    ESP_LOGI(TAG, "Wi-Fi connected, starting cloud stack.");
    mqtt_onenet_start();
    initialize_sntp();
    s_wifi_connected = true;
    farm_display_show_status("WiFi OK", "MQTT OK", "RUNNING");
    switch_led(1);
}

static void publish_sensor_data(const sensor_data_t *sensor_data)
{
    if (!sensor_data)
    {
        return;
    }

    if (s_wifi_connected)
    {
        report_sensor_data(sensor_data->temperature,
                           sensor_data->humidity,
                           sensor_data->nh3_ppm,
                           sensor_data->h2s_ppm,
                           sensor_data->light);
    }

    farm_display_show_sensor(sensor_data);
    control(sensor_data->temperature,
            sensor_data->humidity,
            sensor_data->nh3_ppm,
            sensor_data->h2s_ppm,
            sensor_data->light);
}

static void sensor_task(void *arg)
{
    (void)arg;
    sensor_data_t sensor_data = {0};

    while (1)
    {
        // sim_read_sensors(&sensor_data.temperature,
        //                  &sensor_data.humidity,
        //                  &sensor_data.nh3_ppm,
        //                  &sensor_data.h2s_ppm,
        //                  &sensor_data.light);

        read_sensors(&sensor_data.temperature,
                     &sensor_data.humidity,
                     &sensor_data.nh3_ppm,
                     &sensor_data.h2s_ppm,
                     &sensor_data.light);

        publish_sensor_data(&sensor_data);

        ESP_LOGI("SENSOR",
                 "Temperature: %.2f C, Humidity: %.2f %%, NH3 PPM: %.2f, H2S PPM: %.2f, Light: %d",
                 sensor_data.temperature,
                 sensor_data.humidity,
                 sensor_data.nh3_ppm,
                 sensor_data.h2s_ppm,
                 sensor_data.light);

        vTaskDelay(pdMS_TO_TICKS(2500));
    }
}

static void time_task(void *arg)
{
    (void)arg;
    while (1)
    {
        calc_current_time();
        char time_str[64];
        get_current_time_str(time_str, sizeof(time_str));
        farm_display_show_time(time_str);
        check_AutoTask();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void feed_servo_task(void *arg)
{
    (void)arg;
    while (1)
    {
        if (isLightSystemAutoMode())
        {
            runFeedTask();

            wait_for_interval_update(xTaskGetCurrentTaskHandle(), getAutoFeedIntervalMs);
        }
        else
        {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        }
    }
}

static void clear_servo_task(void *arg)
{
    (void)arg;
    while (1)
    {
        if (isLightSystemAutoMode())
        {
            runClearTask();

            wait_for_interval_update(xTaskGetCurrentTaskHandle(), getAutoClearIntervalMs);
        }
        else
        {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        }
    }
}

void farm_app_start(void)
{
    ESP_LOGI(TAG, "Booting farm application.");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    pwm_init();

    setCurtainTask(true);
    setLightSystemMode(true);

    // 初始化风扇PWM通道
    ESP_ERROR_CHECK(pwm_start(0));
    ESP_ERROR_CHECK(pwm_set_duty(2, 0));
    ESP_ERROR_CHECK(pwm_start(2));

    vTaskDelay(pdMS_TO_TICKS(3000));

    adc_system_init();
    gy30_init();
    farm_display_init();
    neopixel_init(LED_STRIP_PIN, LED_STRIP_LED_NUM);
    switch_led(0);
    switch_light(0);
    switch_heat(0);
    service_router_init();

    // farm_display_show_boot("SmartFarm", "Starting...");
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    wifi_connect_init(on_wifi_connected);

    TaskHandle_t feed_task_handle = NULL;
    TaskHandle_t clear_task_handle = NULL;

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(time_task, "time_task", 4096, NULL, 5, NULL);
    xTaskCreate(feed_servo_task, "feed_servo_task", 4096, NULL, 5, &feed_task_handle);
    xTaskCreate(clear_servo_task, "clear_servo_task", 4096, NULL, 5, &clear_task_handle);

    registerAutoFeedTaskHandle(feed_task_handle);
    registerAutoClearTaskHandle(clear_task_handle);
}