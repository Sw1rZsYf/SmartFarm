/*
 * OneNET MQTT 连接客户端 (基于 ESP-IDF mqtt5_example 简化)
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h" 

static const char *TAG = "ONENET_MQTT";
static esp_mqtt_client_handle_t mqtt_client = NULL;
// OneNET 设备信息配置
#define ONENET_PRODUCT_ID    "h2VKEmBI1g"
#define ONENET_DEVICE_ID     "ESP32_01"
#define ONENET_DEVICE_SECRET "version=2018-10-31&res=products%2Fh2VKEmBI1g%2Fdevices%2FESP32_01&et=1924853811&method=md5&sign=ttQeAl%2BOvweCFsFjLGQP4g%3D%3D"

// OneNET 接入点信息（OneJson协议）
#define ONENET_SERVER_URI    "mqtt://mqtts.heclouds.com:1883" 
#define ONENET_CLIENT_ID     ONENET_DEVICE_ID
#define ONENET_USERNAME      ONENET_PRODUCT_ID

#define ONENET_PASSWORD      ONENET_DEVICE_SECRET   //已经生成好了的token




// MQTT 事件处理函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, ">>> MQTT 成功连接至 OneNET <<<");
        // 订阅 OneNET 命令下发主题
        char subscribe_topic[150];
        snprintf(subscribe_topic, sizeof(subscribe_topic),
                 "$sys/%s/%s/thing/service/+/invoke",
                 ONENET_PRODUCT_ID, ONENET_DEVICE_ID);
        esp_mqtt_client_subscribe(client, subscribe_topic, 1);
        ESP_LOGI(TAG, "已订阅命令主题: %s", subscribe_topic);

        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT 连接断开");
        break;

    case MQTT_EVENT_DATA:
        // 收到平台消息
        ESP_LOGI(TAG, "收到平台消息:");
        ESP_LOGI(TAG, "主题: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "数据: %.*s", event->data_len, event->data);
        // 后续可在此处添加命令解析逻辑
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT 错误事件");
        // 可以在此处打印错误详情，辅助调试
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "传输层错误，errno=%d", event->error_handle->esp_transport_sock_errno);
        }
        break;

    default:
        // 忽略其他不关心的事件
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
        // 设置会话保活时间（秒），OneNET 建议设置
        .session.keepalive = 60,
    };

    // 初始化客户端
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    // 注册事件处理器
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // 启动客户端
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "MQTT 客户端启动指令已发送。");
}


int report_sensor_data(float temperature, float humidity,
                       int nh3_concentration, int h2s_concentration, int light)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT客户端未初始化，无法上报数据");
        return -1;
    }

    // 1. 创建 JSON 根对象
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "创建JSON根对象失败");
        return -1;
    }

    static int msg_id = 1; 
    char id_str[10];
    snprintf(id_str, sizeof(id_str), "%d", msg_id++);
    cJSON_AddStringToObject(root, "id", id_str);
    cJSON_AddStringToObject(root, "version", "1.0");

    // 3. 创建 "params" 对象
    cJSON *params = cJSON_CreateObject();
    if (params == NULL) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "创建JSON params对象失败");
        return -1;
    }
    //形成OneJson报文
    // 温度
    cJSON *temp_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(temp_obj, "value", temperature);
    cJSON_AddItemToObject(params, "temperature", temp_obj);

    // 湿度
    cJSON *humi_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(humi_obj, "value", humidity);
    cJSON_AddItemToObject(params, "humidity", humi_obj);

    // 光照
    cJSON *light_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(light_obj, "value", light);
    cJSON_AddItemToObject(params, "light", light_obj);

    // 硫化氢浓度 (注意：键名必须与物模型标识符完全一致)
    cJSON *h2s_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(h2s_obj, "value", h2s_concentration);
    cJSON_AddItemToObject(params, "h2s_concentration", h2s_obj);

    // 氨气浓度
    cJSON *nh3_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(nh3_obj, "value", nh3_concentration);
    cJSON_AddItemToObject(params, "nh3_concentration", nh3_obj);

    // 5. 将 params 对象挂载到根
    cJSON_AddItemToObject(root, "params", params);

    // 6. 将 cJSON 对象转换为字符串 (使用非格式化以节省空间)
    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "JSON转换字符串失败");
        return -1;
    }

    // 7. 构建上报 Topic
    char publish_topic[150];
    snprintf(publish_topic, sizeof(publish_topic),
             "$sys/%s/%s/thing/property/post",
             ONENET_PRODUCT_ID, ONENET_DEVICE_ID);

    // 8. 发布消息
    int msg_pub_id = esp_mqtt_client_publish(mqtt_client, publish_topic,
                                             json_string, 0, 1, 0);

    if (msg_pub_id < 0) {
        ESP_LOGE(TAG, "MQTT发布失败");
    } else {
        ESP_LOGI(TAG, "数据上报成功，MsgID=%d", msg_pub_id);
        ESP_LOGI(TAG, "Topic: %s", publish_topic);
        ESP_LOGI(TAG, "Data: %s", json_string); // 显示实际发送的数据
    }

    // 9. 释放内存
    free(json_string);
    cJSON_Delete(root);

    return msg_pub_id;
}
