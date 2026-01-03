#include "timer.h"
#include "sntp_time.h"

/**
 * @函数说明        定时器回调函数
 * @传入参数
 * @函数返回
 */
static bool IRAM_ATTR TimerCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    //将传进来的队列保存
    QueueHandle_t queue = (QueueHandle_t)user_data;

    static int time = 0;
    time++;

    //从中断服务程序（ISR）中发送数据到队列
    xQueueSendFromISR(queue, &time, &high_task_awoken);

    return (high_task_awoken == pdTRUE);
}

/**
 * @函数说明        定时器初始化配置
 * @传入参数        resolution_hz=定时器的分辨率  alarm_count=触发回调事件的目标计数值
 * @函数返回        创建的定时器回调队列
 */
QueueHandle_t timerInitConfig(uint32_t resolution_hz, uint64_t alarm_count)
{
    //定义一个通用定时器
    gptimer_handle_t gptimer = NULL;

    //创建一个队列
    QueueHandle_t queue = xQueueCreate(10, sizeof(10));
    //如果创建不成功
    if (!queue) {
        ESP_LOGE("queue", "Creating queue failed");
        return NULL;
    }

    //配置定时器参数
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, //定时器时钟来源 选择APB作为默认选项
        .direction = GPTIMER_COUNT_UP,      //向上计数
        //计数器分辨率(工作频率)以Hz为单位，因此，每个计数滴答的步长等于(1 / resolution_hz)秒
        //假设 resolution_hz = 1000 000
        //1 / resolution_hz = 1 / 1000000 = 0.000001(秒) = 1(微秒) （ 1 tick= 1us ）
        .resolution_hz = resolution_hz,
    };
    //将配置设置到定时器
    gptimer_new_timer(&timer_config, &gptimer);

    //绑定一个回调函数
    gptimer_event_callbacks_t cbs = {
        .on_alarm = TimerCallback,
    };
    //设置定时器gptimer的 回调函数为cbs  传入的参数为NULL
    gptimer_register_event_callbacks(gptimer, &cbs, queue);

    //使能定时器
    gptimer_enable(gptimer);


    //通用定时器的报警值设置
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,  //重载计数值为0
        .alarm_count = alarm_count, // 报警目标计数值 1000000 = 1s
        .flags.auto_reload_on_alarm = true, //开启重加载
    };
    //设置触发报警动作
    gptimer_set_alarm_action(gptimer, &alarm_config);
    //开始定时器开始工作
    gptimer_start(gptimer);

    return queue;
}