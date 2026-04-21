/*
 * service_router.c - OneNET 服务路由层
 */
#include <string.h>
#include "esp_log.h"
#include "gpio_driver.h"
#include "app.h"
#include "mqtt_publisher.h"
#include "service_router.h"

static const char *TAG = "SERVICE_ROUTER";

typedef struct
{
    const char *service_id;
    service_handler_t handler;
} service_route_t;

static void handle_switch_light_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *light_on = cJSON_GetObjectItem(params, "light_on");
    if (!light_on)
    {
        // 兼容旧模型参数名
        light_on = cJSON_GetObjectItem(params, "led_on");
    }

    if (isLightSystemAutoMode())
    {
        mqtt_send_service_reply(service_id, msg_id, 409, "Light system in AUTO mode", NULL);
        return;
    }

    if (cJSON_IsTrue(light_on))
    {
        setLightTask(true);
        mqtt_send_service_reply(service_id, msg_id, 200, "Light turned on", NULL);
    }
    else if (cJSON_IsFalse(light_on))
    {
        setLightTask(false);
        mqtt_send_service_reply(service_id, msg_id, 200, "Light turned off", NULL);
    }
    else
    {
        mqtt_send_service_reply(service_id, msg_id, 400, "Missing light_on", NULL);
    }
}

static void handle_switch_fan_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *fan_on = cJSON_GetObjectItem(params, "fan_on");

    if (isLightSystemAutoMode())
    {
        mqtt_send_service_reply(service_id, msg_id, 409, "Light system in AUTO mode", NULL);
        return;
    }

    if (cJSON_IsTrue(fan_on))
    {
        setCurtainTask(true);
        ESP_LOGI(TAG, "执行命令: 打开窗帘(复用switch_fan服务)");
        mqtt_send_service_reply(service_id, msg_id, 200, "Curtain opened", NULL);
    }
    else if (cJSON_IsFalse(fan_on))
    {
        setCurtainTask(false);
        ESP_LOGI(TAG, "执行命令: 关闭窗帘(复用switch_fan服务)");
        mqtt_send_service_reply(service_id, msg_id, 200, "Curtain closed", NULL);
    }
    else
    {
        mqtt_send_service_reply(service_id, msg_id, 400, "Missing fan_on", NULL);
    }
}

static void handle_curtain_mode_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *mode_on = cJSON_GetObjectItem(params, "Light_mode");
    if (!mode_on)
    {
        mode_on = cJSON_GetObjectItem(params, "light_mode");
    }

    if (cJSON_IsTrue(mode_on))
    {
        setLightSystemMode(true);
        mqtt_send_service_reply(service_id, msg_id, 200, "Light mode set to AUTO", NULL);
    }
    else if (cJSON_IsFalse(mode_on))
    {
        setLightSystemMode(false);
        mqtt_send_service_reply(service_id, msg_id, 200, "Light mode set to MANUAL", NULL);
    }
    else
    {
        mqtt_send_service_reply(service_id, msg_id, 400, "Missing Light_mode", NULL);
    }
}

static void handle_heat_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *heat_on = cJSON_GetObjectItem(params, "heat_on");

    if (isLightSystemAutoMode())
    {
        mqtt_send_service_reply(service_id, msg_id, 409, "Light system in AUTO mode", NULL);
        return;
    }

    if (cJSON_IsTrue(heat_on))
    {
        setHeatTask(true);
        mqtt_send_service_reply(service_id, msg_id, 200, "Heat turned on", NULL);
    }
    else if (cJSON_IsFalse(heat_on))
    {
        setHeatTask(false);
        mqtt_send_service_reply(service_id, msg_id, 200, "Heat turned off", NULL);
    }
    else
    {
        mqtt_send_service_reply(service_id, msg_id, 400, "Missing heat_on", NULL);
    }
}

static void handle_feed_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *feed_on = cJSON_GetObjectItem(params, "feed_on");

    if (isLightSystemAutoMode())
    {
        mqtt_send_service_reply(service_id, msg_id, 409, "System in AUTO mode", NULL);
        return;
    }

    if (cJSON_IsTrue(feed_on))
    {
        runFeedTask();
        mqtt_send_service_reply(service_id, msg_id, 200, "Feed triggered", NULL);
    }
    else if (cJSON_IsFalse(feed_on))
    {
        mqtt_send_service_reply(service_id, msg_id, 200, "Feed off", NULL);
    }
    else
    {
        mqtt_send_service_reply(service_id, msg_id, 400, "Missing feed_on", NULL);
    }
}

static void handle_clear_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *clear_on = cJSON_GetObjectItem(params, "clear_on");

    if (isLightSystemAutoMode())
    {
        mqtt_send_service_reply(service_id, msg_id, 409, "System in AUTO mode", NULL);
        return;
    }

    if (cJSON_IsTrue(clear_on))
    {
        runClearTask();
        mqtt_send_service_reply(service_id, msg_id, 200, "Clear triggered", NULL);
    }
    else if (cJSON_IsFalse(clear_on))
    {
        mqtt_send_service_reply(service_id, msg_id, 200, "Clear off", NULL);
    }
    else
    {
        mqtt_send_service_reply(service_id, msg_id, 400, "Missing clear_on", NULL);
    }
}

static void handle_set_time_service(const cJSON *params, const char *msg_id, const char *service_id)
{
    cJSON *feed_time_json = cJSON_GetObjectItem(params, "Feed_time");
    cJSON *clear_time_json = cJSON_GetObjectItem(params, "Clear_time");

    if (!feed_time_json)
    {
        feed_time_json = cJSON_GetObjectItem(params, "feed_time");
    }

    if (!clear_time_json)
    {
        clear_time_json = cJSON_GetObjectItem(params, "clear_time");
    }

    if (!cJSON_IsNumber(feed_time_json) || !cJSON_IsNumber(clear_time_json))
    {
        mqtt_send_service_reply(service_id, msg_id, 400, "Missing Feed_time/Clear_time", NULL);
        return;
    }

    if (feed_time_json->valuedouble < 1.0 || clear_time_json->valuedouble < 1.0)
    {
        mqtt_send_service_reply(service_id, msg_id, 400, "Feed_time/Clear_time must be >= 1s", NULL);
        return;
    }

    uint32_t feed_interval_sec = (uint32_t)feed_time_json->valuedouble;
    uint32_t clear_interval_sec = (uint32_t)clear_time_json->valuedouble;

    setAutoIntervalsSec(feed_interval_sec, clear_interval_sec);

    ESP_LOGI(TAG, "更新自动间隔: Feed=%lus, Clear=%lus",
             (unsigned long)feed_interval_sec,
             (unsigned long)clear_interval_sec);
    mqtt_send_service_reply(service_id, msg_id, 200, "Auto intervals updated", NULL);
}

static const service_route_t s_builtin_routes[] = {
    {"switch_light", handle_switch_light_service},
    {"switch_led", handle_switch_light_service},
    {"switch_fan", handle_switch_fan_service},
    {"curtain_mode", handle_curtain_mode_service},
    {"heat", handle_heat_service},
    {"Feed", handle_feed_service},
    {"clear_on", handle_clear_service},
    {"setAutoFeedTime", handle_set_time_service},
};

void service_router_init(void)
{
    service_router_register_builtin_services();
}

void service_router_register_builtin_services(void)
{
    // 当前为静态路由表，初始化留作扩展点
}

int service_router_dispatch(const char *service_id, const cJSON *params, const char *msg_id)
{
    if (!service_id || !params)
    {
        return -1;
    }

    for (size_t i = 0; i < sizeof(s_builtin_routes) / sizeof(s_builtin_routes[0]); ++i)
    {
        if (strcmp(service_id, s_builtin_routes[i].service_id) == 0)
        {
            s_builtin_routes[i].handler(params, msg_id, service_id);
            return 0;
        }
    }

    ESP_LOGW(TAG, "未知服务ID: %s", service_id);
    mqtt_send_service_reply(service_id, msg_id, 404, "Service not found", NULL);
    return -1;
}