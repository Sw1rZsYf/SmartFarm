/*
 * mqtt_handler.h - MQTT事件处理接口
 */
#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "mqtt_client.h"

// MQTT事件处理函数声明
void mqtt_handle_connected_event(esp_mqtt_client_handle_t client);
void mqtt_handle_disconnected_event(void);
void mqtt_handle_data_event(esp_mqtt_event_handle_t event);
void mqtt_handle_error_event(esp_mqtt_event_handle_t event);

#endif // MQTT_HANDLER_H