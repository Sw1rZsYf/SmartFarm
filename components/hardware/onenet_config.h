/*
 * onenet_config.h - OneNET配置头文件
 */
#ifndef ONENET_CONFIG_H
#define ONENET_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

// OneNET 设备信息配置
#define ONENET_PRODUCT_ID "h2VKEmBI1g"
#define ONENET_DEVICE_ID "ESP32_01"
#define ONENET_DEVICE_SECRET "version=2018-10-31&res=products%2Fh2VKEmBI1g%2Fdevices%2FESP32_01&et=1924853811&method=md5&sign=ttQeAl%2BOvweCFsFjLGQP4g%3D%3D"

// OneNET 接入点信息
#define ONENET_SERVER_URI "mqtt://mqtts.heclouds.com:1883"
#define ONENET_CLIENT_ID ONENET_DEVICE_ID
#define ONENET_USERNAME ONENET_PRODUCT_ID
#define ONENET_PASSWORD ONENET_DEVICE_SECRET

    // 函数声明
    void mqtt_onenet_start(void);
    int report_sensor_data(float temperature, float humidity,
                           int nh3_concentration, int h2s_concentration, int light);

#ifdef __cplusplus
}
#endif

#endif /* ONENET_CONFIG_H */