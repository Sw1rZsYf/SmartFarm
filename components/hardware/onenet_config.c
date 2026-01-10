/*
 * OneNET MQTT 连接客户端 (重构版)
 */
#include <stdio.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "onenet_config.h"
#include "mqtt_handler.h"
#include "mqtt_publisher.h"

static const char *TAG = "ONENET_MQTT";
static esp_mqtt_client_handle_t mqtt_client = NULL;

// MQTT 事件处理函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        mqtt_handle_connected_event(event->client);
        break;

    case MQTT_EVENT_DISCONNECTED:
        mqtt_handle_disconnected_event();
        break;

    case MQTT_EVENT_DATA:
        mqtt_handle_data_event(event);
        break;

    case MQTT_EVENT_ERROR:
        mqtt_handle_error_event(event);
        break;

    default:
        // 忽略其他事件
        break;
    }
}

// 启动 OneNET MQTT 客户端
void mqtt_onenet_start(void)
{
    ESP_LOGI(TAG, "正在启动 OneNET MQTT 客户端...");

    // MQTT 客户端配置
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = ONENET_SERVER_URI,
        .credentials.client_id = ONENET_CLIENT_ID,
        .credentials.username = ONENET_USERNAME,
        .credentials.authentication.password = ONENET_PASSWORD,
        .session.keepalive = 60,
    };

    // 初始化客户端
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    // 注册事件处理器
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    // 初始化发布器
    mqtt_publisher_init(mqtt_client);

    // 启动客户端
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "MQTT 客户端启动完成");
}

// 上报传感器数据（包装函数）
int report_sensor_data(float temperature, float humidity,
                       int nh3_concentration, int h2s_concentration, int light)
{
    return mqtt_report_sensor_data(temperature, humidity,
                                   nh3_concentration, h2s_concentration, light);
}