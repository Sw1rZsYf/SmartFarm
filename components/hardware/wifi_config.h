#ifndef _WIFI_CONFIG_H__
#define _WIFI_CONFIG_H_

#include "esp_event.h"




// 声明一个回调函数类型，用于在Wi-Fi连接成功后，通知主程序去启动MQTT等。
typedef void (*wifi_connected_callback_t)(void);

// 初始化Wi-Fi并开始连接，传入一个成功后的回调函数。
void wifi_connect_init(wifi_connected_callback_t on_connected);


#endif /* _WIFI_CONFIG_H__ */