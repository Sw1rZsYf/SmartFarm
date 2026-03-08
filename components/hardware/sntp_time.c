#include "sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sntp_time.h"
#include "app.h"

static const char *TIME_TAG = "SNTP";

// 时区设置，北京时间 (东八区)
#define TIME_ZONE "CST-8"
// SNTP服务器（中国区可用）
#define NTP_SERVER "ntp1.aliyun.com"

// 全局变量当前时间
struct tm current_time;

// SNTP初始化函数
void initialize_sntp(void)
{
    ESP_LOGI(TIME_TAG, "正在初始化SNTP...");

    // 设置时区
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    // 配置SNTP
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);

    // 设置多个NTP服务器
    esp_sntp_setservername(0, "ntp1.aliyun.com"); // 阿里云
    esp_sntp_setservername(1, "ntp.ntsc.ac.cn");  // 国家授时中心
    esp_sntp_setservername(2, "cn.pool.ntp.org"); // NTP池
    esp_sntp_setservername(3, "203.107.6.88");    // 阿里云IP

    // 可选：注册同步回调
    // sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // 初始化SNTP
    esp_sntp_init();

    // 等待时间同步（检查实际时间）
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int max_retries = 60; // 60次 * 500ms = 30秒
    const int delay_ms = 500;

    time(&now);
    localtime_r(&now, &timeinfo);

    while ((timeinfo.tm_year < (2020 - 1900)) && retry < max_retries)
    {
        ESP_LOGI(TIME_TAG, "等待时间同步... (%d/%d)", retry + 1, max_retries);
        retry++;
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);

        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year >= (2020 - 1900))
    {
        ESP_LOGI(TIME_TAG, "时间同步成功！当前时间: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else
    {
        ESP_LOGW(TIME_TAG, "SNTP同步超时！定时功能将被关闭！");
    }
}

void calc_current_time(void)
{
    // 获取并打印当前时间
    time_t now;
    time(&now);
    localtime_r(&now, &current_time);
    // 检查有无自动任务需要执行
    check_AutoTask();

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
