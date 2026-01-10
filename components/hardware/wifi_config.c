#include "wifi_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "WiFi_Connector";

// // Wi-Fi账号密码
// #define WIFI_SSID      "Xiaomi_818F"
// #define WIFI_PASSWORD  "wwqy666."
// #define MAX_RETRY_NUM  5

// Wi-Fi账号密码
#define WIFI_SSID      "Magic"
#define WIFI_PASSWORD  "zjy1234567"
#define MAX_RETRY_NUM  5

// #define WIFI_SSID      "HONOR-310I2C"
// #define WIFI_PASSWORD  "12345678lcw."
// #define MAX_RETRY_NUM  5

// 事件组和回调函数句柄
static EventGroupHandle_t s_wifi_event_group;
static wifi_connected_callback_t s_connected_callback = NULL;
static int s_retry_num = 0;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// 事件处理函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Wi-Fi station started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // 处理断开重连
        if (s_retry_num < MAX_RETRY_NUM) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Disconnected, retry (%d/%d)", s_retry_num, MAX_RETRY_NUM);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect after %d retries", MAX_RETRY_NUM);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // 成功获取IP，连接完成
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        if (s_connected_callback) {
            s_connected_callback();
        }
    }
}

// 初始化，外部调用，链接wifi
void wifi_connect_init(wifi_connected_callback_t on_connected)
{
    // 保存回调函数，用于连接成功后通知主程序
    s_connected_callback = on_connected;
    
    // 初始化事件组
    s_wifi_event_group = xEventGroupCreate();
    
    // 初始化TCP/IP和默认事件循环（如果主程序已初始化过，此部分可移至主程序，确保只执行一次）
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    
    // 初始化Wi-Fi驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    // 配置并启动Wi-Fi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 使用最通用的WPA2认证
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Wi-Fi initialization finished, waiting for connection...");
    
}