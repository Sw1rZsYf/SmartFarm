#ifndef _TIMER_H
#define _TIMER_H

#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"

/**
 * @函数说明        定时器初始化配置
 * @传入参数        resolution_hz=定时器的分辨率  alarm_count=触发警报事件的目标计数值
 * @函数返回        创建的定时器回调队列
 */
QueueHandle_t timerInitConfig(uint32_t resolution_hz, uint64_t alarm_count);




#endif // _TIMER_H