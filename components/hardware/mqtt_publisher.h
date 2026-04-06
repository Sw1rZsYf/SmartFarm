/*
 * mqtt_publisher.h - MQTT数据上报接口
 */
#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include "mqtt_client.h"
#include "cJSON.h"

// 初始化发布器
void mqtt_publisher_init(esp_mqtt_client_handle_t client);

// 发送服务响应
void mqtt_send_service_reply(const char *service_id, const char *id,
                             int code, const char *msg, cJSON *data);

// 上报传感器数据
int mqtt_report_sensor_data(float temperature, float humidity,
                            float nh3_concentration, float h2s_concentration, int light);

// 获取最近一次属性上报的业务ID（JSON中的id）
const char *mqtt_get_last_report_id(void);

#endif // MQTT_PUBLISHER_H