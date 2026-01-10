/*
 * mqtt_publisher.c - MQTT数据上报功能
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "onenet_config.h"
#include "mqtt_publisher.h"

static const char *TAG = "MQTT_PUBLISHER";

static esp_mqtt_client_handle_t mqtt_client = NULL;

// 设置MQTT客户端句柄
void mqtt_publisher_init(esp_mqtt_client_handle_t client)
{
    mqtt_client = client;
}

// 创建JSON参数对象
static cJSON *create_param_object(const char *key, double value)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj)
    {
        cJSON_AddNumberToObject(obj, "value", value);
    }
    return obj;
}

// 发送服务响应
void mqtt_send_service_reply(const char *service_id, const char *id,
                             int code, const char *msg, cJSON *data)
{
    if (!mqtt_client)
    {
        ESP_LOGE(TAG, "MQTT客户端未初始化");
        return;
    }

    // 构建回复主题
    char reply_topic[150];
    snprintf(reply_topic, sizeof(reply_topic),
             "$sys/%s/%s/thing/service/%s/invoke_reply",
             ONENET_PRODUCT_ID, ONENET_DEVICE_ID, service_id);

    // 创建回复JSON
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return;

    cJSON_AddStringToObject(root, "id", id ? id : "0");
    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddStringToObject(root, "msg", msg ? msg : "");

    if (data)
    {
        cJSON_AddItemToObject(root, "data", data);
    }
    else
    {
        cJSON *empty_data = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "data", empty_data);
    }

    // 发送消息
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str)
    {
        int msg_id = esp_mqtt_client_publish(mqtt_client, reply_topic,
                                             json_str, 0, 1, 0);
        ESP_LOGI(TAG, "服务响应发送，MsgID=%d, Topic=%s", msg_id, reply_topic);
        free(json_str);
    }

    cJSON_Delete(root);
}

// 上报传感器数据
int mqtt_report_sensor_data(float temperature, float humidity,
                            int nh3_concentration, int h2s_concentration, int light)
{
    if (!mqtt_client)
    {
        ESP_LOGE(TAG, "MQTT客户端未初始化，无法上报数据");
        return -1;
    }

    static int msg_id_counter = 1;
    char id_str[10];
    snprintf(id_str, sizeof(id_str), "%d", msg_id_counter++);

    // 创建JSON根对象
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        ESP_LOGE(TAG, "创建JSON根对象失败");
        return -1;
    }

    cJSON_AddStringToObject(root, "id", id_str);
    cJSON_AddStringToObject(root, "version", "1.0");

    // 创建params对象
    cJSON *params = cJSON_CreateObject();
    if (!params)
    {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "创建JSON params对象失败");
        return -1;
    }

    // 添加各个传感器数据
    cJSON *temp_obj = create_param_object("value", temperature);
    cJSON *humi_obj = create_param_object("value", humidity);
    cJSON *light_obj = create_param_object("value", light);
    cJSON *h2s_obj = create_param_object("value", h2s_concentration);
    cJSON *nh3_obj = create_param_object("value", nh3_concentration);

    if (temp_obj && humi_obj && light_obj && h2s_obj && nh3_obj)
    {
        cJSON_AddItemToObject(params, "temperature", temp_obj);
        cJSON_AddItemToObject(params, "humidity", humi_obj);
        cJSON_AddItemToObject(params, "light", light_obj);
        cJSON_AddItemToObject(params, "h2s_concentration", h2s_obj);
        cJSON_AddItemToObject(params, "nh3_concentration", nh3_obj);
    }

    cJSON_AddItemToObject(root, "params", params);

    // 转换为JSON字符串
    char *json_string = cJSON_PrintUnformatted(root);
    if (!json_string)
    {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "JSON转换字符串失败");
        return -1;
    }

    // 构建上报主题
    char publish_topic[150];
    snprintf(publish_topic, sizeof(publish_topic),
             "$sys/%s/%s/thing/property/post",
             ONENET_PRODUCT_ID, ONENET_DEVICE_ID);

    // 发布消息
    int msg_pub_id = esp_mqtt_client_publish(mqtt_client, publish_topic,
                                             json_string, 0, 1, 0);

    if (msg_pub_id < 0)
    {
        ESP_LOGE(TAG, "MQTT发布失败");
    }
    else
    {
        ESP_LOGI(TAG, "数据上报成功，MsgID=%d", msg_pub_id);
        ESP_LOGI(TAG, "Topic: %s", publish_topic);
        ESP_LOGI(TAG, "Data: %s", json_string);
    }

    // 清理资源
    free(json_string);
    cJSON_Delete(root);

    return msg_pub_id;
}