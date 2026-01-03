#include "sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TIME_TAG = "SNTP";

// 时区设置，北京时间 (东八区)
#define TIME_ZONE "CST-8"
// SNTP服务器（中国区可用）
#define NTP_SERVER "ntp1.aliyun.com"

//全局变量当前时间
struct tm current_time;


// SNTP初始化函数
void initialize_sntp(void)
{
    ESP_LOGI(TIME_TAG, "正在初始化SNTP...");
    
    // 设置时区
    setenv("TZ", TIME_ZONE, 1);
    tzset();
    
    // 配置SNTP运行模式（等待同步后再继续）
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    
    // 设置NTP服务器地址
    esp_sntp_setservername(0, NTP_SERVER);
    
    // 可选：设置备用服务器
    esp_sntp_setservername(1, "ntp.ntsc.ac.cn");
    esp_sntp_setservername(2, "pool.ntp.org");
    
    // 初始化SNTP服务
    esp_sntp_init();
    
    // 等待时间同步（最多等待10秒）
    int retry = 0;
    const int retry_count = 20; // 20 * 500ms = 10秒
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < retry_count) {
        ESP_LOGI(TIME_TAG, "等待时间同步... (%d/%d)", retry+1, retry_count);
        retry++;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    if (retry == retry_count) {
        ESP_LOGW(TIME_TAG, "SNTP同步超时！ 定时功能将被关闭！");
    } 
}

void calc_current_time(void)
{
    // 获取并打印当前时间
    time_t now;
    time(&now);
    localtime_r(&now, &current_time);
    ESP_LOGI(TIME_TAG, "时间同步成功！当前时间: %04d-%02d-%02d %02d:%02d:%02d",
                current_time.tm_year + 1900, current_time.tm_mon + 1, current_time.tm_mday,
                current_time.tm_hour, current_time.tm_min, current_time.tm_sec);
    
    // sntp_stop();
}


uint8_t get_current_hour(void)
{
    return current_time.tm_hour;
}
uint8_t get_current_minute(void)
{
    return current_time.tm_min;
}
