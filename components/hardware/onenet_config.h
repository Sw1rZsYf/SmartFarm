// onenet_config.h
#ifndef ONENET_CONFIG_H
#define ONENET_CONFIG_H

#include <stdbool.h>

// 主题格式确认（根据OneNET OneJson文档）


// 启动 OneNET MQTT 客户端
void mqtt_onenet_start(void);
// 上报传感器数据到 OneNET (实测可用格式)
int report_sensor_data(float temperature, float humidity,
                       int nh3_concentration, int h2s_concentration, int light);

#endif