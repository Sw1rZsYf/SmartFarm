/*
 * mqtt_handler.c - MQTT事件处理和命令解析
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "gpio_driver.h"
#include "onenet_config.h"
#include "mqtt_handler.h"
#include "mqtt_publisher.h"
#include "app.h"

static const char *TAG = "MQTT_HANDLER";

// 主题字符串生成辅助函数
static char *build_topic(const char *format, const char *product_id, const char *device_id, const char *extra)
{
    static char topic[200];
    if (extra)
    {
        snprintf(topic, sizeof(topic), format, product_id, device_id, extra);
    }
    else
    {
        snprintf(topic, sizeof(topic), format, product_id, device_id);
    }
    return topic;
}

// 解析服务调用主题，提取 service_id
static char *parse_service_id(const char *topic)
{
    static char service_id[50];
    memset(service_id, 0, sizeof(service_id));

    const char *service_start = strstr(topic, "thing/service/");
    if (!service_start)
        return NULL;

    service_start += strlen("thing/service/");
    const char *invoke_start = strstr(service_start, "/invoke");
    if (!invoke_start)
        return NULL;

    int len = invoke_start - service_start;
    if (len > 0 && len < (int)sizeof(service_id))
    {
        strncpy(service_id, service_start, len);
        service_id[len] = '\0';
        return service_id;
    }
    return NULL;
}

// 处理开关灯服务
static void handle_switch_led_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *led_on = cJSON_GetObjectItem(params, "led_on");

    if (cJSON_IsTrue(led_on))
    {
        runOnLedTask();
        mqtt_send_service_reply(service_id, msg_id, 200, "LED turned on", NULL);
    }
    else if (cJSON_IsFalse(led_on))
    {
        runOffLedTask();
        mqtt_send_service_reply(service_id, msg_id, 200, "LED turned off", NULL);
    }
}

// 处理开关风扇服务
static void handle_switch_fan_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *fan_on = cJSON_GetObjectItem(params, "fan_on");

    if (cJSON_IsTrue(fan_on))
    {
        switch_fan(1);
        ESP_LOGI(TAG, "执行命令: 开风扇");
        mqtt_send_service_reply(service_id, msg_id, 200, "FAN turned on", NULL);
    }
    else if (cJSON_IsFalse(fan_on))
    {
        switch_fan(0);
        ESP_LOGI(TAG, "执行命令: 关风扇");
        mqtt_send_service_reply(service_id, msg_id, 200, "FAN turned off", NULL);
    }
}

static void handle_set_time_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *hour_json = cJSON_GetObjectItem(params, "hour");
    cJSON *min_json = cJSON_GetObjectItem(params, "min");
    int hour = hour_json->valueint;
    int min = min_json->valueint;
    setFeedTask(hour, min, 1);
    mqtt_send_service_reply(service_id, msg_id, 200, "OK", NULL);
}

// 处理服务调用
static void process_service_invoke(const char *topic, const char *data)
{
    ESP_LOGI(TAG, "解析到服务调用命令");

    char *service_id = parse_service_id(topic);
    if (!service_id)
    {
        ESP_LOGE(TAG, "无法从主题中提取service_id");
        return;
    }

    cJSON *root = cJSON_Parse(data);
    if (!root)
    {
        ESP_LOGE(TAG, "JSON解析失败");
        return;
    }

    cJSON *params = cJSON_GetObjectItem(root, "params");
    cJSON *msg_id_json = cJSON_GetObjectItem(root, "id");
    char *msg_id = msg_id_json ? msg_id_json->valuestring : "0";

    if (params && cJSON_IsObject(params))
    {
        if (strcmp(service_id, "switch_led") == 0)
        {
            handle_switch_led_service(params, msg_id, service_id);
        }
        else if (strcmp(service_id, "switch_fan") == 0)
        {
            handle_switch_fan_service(params, msg_id, service_id);
        }
        else if (strcmp(service_id, "setAutoFeedTime") == 0)
        {
            handle_set_time_service(params, msg_id, service_id);
        }
        else
        {
            ESP_LOGW(TAG, "未知服务ID: %s", service_id);
            mqtt_send_service_reply(service_id, msg_id, 404, "Service not found", NULL);
        }
    }

    cJSON_Delete(root);
}

// 处理MQTT数据事件
void mqtt_handle_data_event(esp_mqtt_event_handle_t event)
{
    // 提取主题和数据
    char topic[event->topic_len + 1];
    char data[event->data_len + 1];

    memcpy(topic, event->topic, event->topic_len);
    topic[event->topic_len] = '\0';
    memcpy(data, event->data, event->data_len);
    data[event->data_len] = '\0';

    ESP_LOGI(TAG, "收到主题: %s", topic);
    ESP_LOGI(TAG, "收到数据: %s", data);

    // 判断是否为服务调用主题
    if (strstr(topic, "/thing/service/") != NULL &&
        strstr(topic, "/invoke") != NULL)
    {
        process_service_invoke(topic, data);
    }
    else
    {
        ESP_LOGD(TAG, "非服务调用主题，已忽略");
    }
}

// 处理MQTT连接事件
void mqtt_handle_connected_event(esp_mqtt_client_handle_t client)
{
    ESP_LOGI(TAG, ">>> MQTT 成功连接至 OneNET <<<");

    // 构建订阅主题
    char *subscribe_topic = build_topic("$sys/%s/%s/thing/service/+/invoke",
                                        ONENET_PRODUCT_ID, ONENET_DEVICE_ID, NULL);

    // 订阅命令下发主题
    esp_mqtt_client_subscribe(client, subscribe_topic, 1);
    ESP_LOGI(TAG, "已订阅命令主题: %s", subscribe_topic);
}

// 处理MQTT断开事件
void mqtt_handle_disconnected_event(void)
{
    ESP_LOGI(TAG, "MQTT 连接断开");
}

// 处理MQTT错误事件
void mqtt_handle_error_event(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT 错误事件");

    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
    {
        ESP_LOGE(TAG, "传输层错误，errno=%d",
                 event->error_handle->esp_transport_sock_errno);
    }
}