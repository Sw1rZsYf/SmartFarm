#include "app.h"
#include "dht.h"
#include "gy30.h"
#include "sntp_time.h"
#include "pwm_driver.h"
#include "tjc_driver.h"
#include <math.h>
static const char *TAG = "APP_TASK";

#define LIGHT_THRESHOLD_LOW 80   // 光照低于此值（lux）时打开窗帘（增加光照）
#define LIGHT_THRESHOLD_HIGH 500 // 光照高于此值（lux）时关闭窗帘（减少光照）

// MQ 传感器计算参数（基于手册典型曲线，建议后续按标气重新标定）
#define GAS_SUPPLY_VOLTAGE 5.0f
#define GAS_LOAD_RESISTOR_KOHM 4.7f

#define NH3_R0_KOHM 20.0f
#define H2S_R0_KOHM 20.0f

// ppm = A * (Rs/R0)^B
#define NH3_CURVE_A 50.0f
#define NH3_CURVE_B (-2.30f)
#define H2S_CURVE_A 82.0f
#define H2S_CURVE_B (-2.04f)

#define NH3_FAN_FULL_PPM 15.0f
#define H2S_FAN_FULL_PPM 8.0f
#define GAS_WARMUP_MS 60000

#define FEED_INTERVAL_DEFAULT_SEC 20U
#define CLEAR_INTERVAL_DEFAULT_SEC 30U
#define AUTO_INTERVAL_MIN_SEC 1U
#define AUTO_INTERVAL_MAX_SEC 100000U

// 初始化默认值
farm_task_t TaskFeed =
    {
        .hour = 15,
        .min = 0,
        .mode = Manual,
        .flag = 0

};
farm_task_t TaskEmssion =
    {
        .hour = 15,
        .min = 0,
        .mode = Manual,
        .flag = 0

};
bool CurtainOpen = false;
static int s_last_fan_percent = -1;
static bool s_last_heat_on = false;
static bool s_last_light_on = false;
static bool s_light_auto_mode = true;
static bool s_wait_high_light_recheck = false;
static bool s_curtain_closed_by_high_light = false;
static bool s_gas_warmup_skip_logged = false;
static bool s_gas_warmup_done_logged = false;
static uint32_t s_auto_feed_interval_ms = FEED_INTERVAL_DEFAULT_SEC * 1000U;
static uint32_t s_auto_clear_interval_ms = CLEAR_INTERVAL_DEFAULT_SEC * 1000U;
static TaskHandle_t s_auto_feed_task_handle = NULL;
static TaskHandle_t s_auto_clear_task_handle = NULL;

static void apply_curtain_state(bool open, const char *reason);

static float mq_calc_rs_kohm_from_vrl_mv(int vrl_mv)
{
    float vrl = (float)vrl_mv / 1000.0f;
    if (vrl < 0.01f)
    {
        vrl = 0.01f;
    }
    if (vrl > (GAS_SUPPLY_VOLTAGE - 0.01f))
    {
        vrl = GAS_SUPPLY_VOLTAGE - 0.01f;
    }

    return GAS_LOAD_RESISTOR_KOHM * (GAS_SUPPLY_VOLTAGE - vrl) / vrl;
}

static float mq_calc_ppm_from_rs(float rs_kohm, float r0_kohm, float curve_a, float curve_b)
{
    if (r0_kohm <= 0.01f)
    {
        return 0.0f;
    }

    float ratio = rs_kohm / r0_kohm;
    if (ratio <= 0.0f)
    {
        return 0.0f;
    }

    float ppm = curve_a * powf(ratio, curve_b);
    return ppm > 0.0f ? ppm : 0.0f;
}

static void apply_heat_state(bool on, const char *reason)
{
    if (on != s_last_heat_on)
    {
        switch_heat(on ? 1 : 0);
        s_last_heat_on = on;
        ESP_LOGI(TAG, "加热状态更新: %s, reason=%s", on ? "ON" : "OFF", reason ? reason : "N/A");
    }
}

static void apply_light_state(bool on, const char *reason)
{
    if (on != s_last_light_on)
    {
        switch_light(on ? 1 : 0);
        s_last_light_on = on;
        ESP_LOGI(TAG, "补光灯状态更新: %s, reason=%s", on ? "ON" : "OFF", reason ? reason : "N/A");
    }
}

void read_sensors(float *temperature, float *humidity,
                  float *nh3_ppm, float *h2s_ppm, int *light)
{
    int nh3_voltage, h2s_voltage;
    dht_read_float_data(DHT_TYPE_DHT11, DHT_GPIO_PIN, humidity, temperature);

    adc_system_read(&nh3_voltage, &h2s_voltage, light);

    float nh3_rs = mq_calc_rs_kohm_from_vrl_mv(nh3_voltage);
    float h2s_rs = mq_calc_rs_kohm_from_vrl_mv(h2s_voltage);

    *nh3_ppm = mq_calc_ppm_from_rs(nh3_rs, NH3_R0_KOHM, NH3_CURVE_A, NH3_CURVE_B);
    *h2s_ppm = mq_calc_ppm_from_rs(h2s_rs, H2S_R0_KOHM, H2S_CURVE_A, H2S_CURVE_B);

    ESP_LOGD(TAG, "Gas ADC/PPM: NH3=%dmV Rs=%.2fk R0=%.2fk -> %.2fppm, H2S=%dmV Rs=%.2fk R0=%.2fk -> %.2fppm",
             nh3_voltage, nh3_rs, NH3_R0_KOHM, *nh3_ppm,
             h2s_voltage, h2s_rs, H2S_R0_KOHM, *h2s_ppm);

    *light = (int)gy30_read_light();
}

void sim_read_sensors(float *temperature, float *humidity,
                      float *nh3_ppm, float *h2s_ppm, int *light)
{
    // 模拟传感器数据
    *temperature = 25.0 + (rand() % 100) / 10.0; // 25.0 到 35.0 度
    *humidity = 40.0 + (rand() % 600) / 10.0;    // 40.0% 到 100.0%
    // *nh3_ppm = 1.0 * (200 + (rand() % 800)) / 2000.0; // 200 到 1000 mV
    // *h2s_ppm = 1.0 * (150 + (rand() % 850)) / 2000.0; // 150 到 1000 mV
    *light = 100 + (rand() % 900); // 100 到 1000 lx
}

void setFeedTask(int hour, int min, int mode)
{
    if (mode == 0)
    {
        TaskFeed.mode = 0;
        runFeedTask();
    }
    else if (mode == 1)
    {
        TaskFeed.hour = hour;
        TaskFeed.min = min;
        TaskFeed.mode = mode;
        ESP_LOGI(TAG, "设置自动投料时间为：%d点%d分", hour, min);
        TaskFeed.flag = 1;
    }
}

void setEmssionTask(int hour, int min, int mode)
{
    if (mode == 0)
    {
        TaskEmssion.mode = 0;
        runEmssionTask();
    }
    else if (mode == 1)
    {
        TaskEmssion.hour = hour;
        TaskEmssion.min = min;
        TaskEmssion.mode = mode;
        ESP_LOGI(TAG, "设置自动排放时间为：%d点%d分", hour, min);
        TaskEmssion.flag = 1;
    }
}

void check_AutoTask()
{
    uint8_t current_hour = get_current_hour();
    uint8_t current_min = get_current_minute();

    if (TaskFeed.mode == 1 && current_hour >= TaskFeed.hour && current_min >= TaskFeed.min && TaskFeed.flag == 1)
    {
        runFeedTask();
        TaskFeed.flag = 0; // 执行一次后重置标志，避免重复执行
    }

    if (TaskEmssion.mode == 1 && current_hour >= TaskEmssion.hour && current_min >= TaskEmssion.min && TaskEmssion.flag == 1)
    {
        runEmssionTask();
        TaskEmssion.flag = 0; // 执行一次后重置标志，避免重复执行
    }
}
// 排粪
void runEmssionTask()
{
    ESP_LOGI(TAG, "开始排放！");
}
// 喂食
void runFeedTask()
{
    ESP_LOGI(TAG, "开始投料！");
    sg90_set_angle_smooth(1, 45, 5, 40);
    vTaskDelay(pdMS_TO_TICKS(300));
    sg90_set_angle_smooth(1, 0, 5, 40);
}

void runClearTask(void)
{
    ESP_LOGI(TAG, "开始清粪！");
    sg90_set_angle_smooth(3, 45, 5, 40);
    vTaskDelay(pdMS_TO_TICKS(300));
    sg90_set_angle_smooth(3, 0, 5, 40);
}

void runOnLedTask(void)
{
    setLightTask(true);
    ESP_LOGI(TAG, "执行命令: 开补光灯");
}

void runOffLedTask(void)
{
    setLightTask(false);
    ESP_LOGI(TAG, "执行命令: 关补光灯");
}

void setLightTask(bool on)
{
    apply_light_state(on, "cloud_service");
}

void setHeatTask(bool on)
{
    apply_heat_state(on, "cloud_service");
}

void setCurtainTask(bool open)
{
    apply_curtain_state(open, "cloud_service");
}

void setLightSystemMode(bool auto_mode)
{
    s_light_auto_mode = auto_mode;
    ESP_LOGI(TAG, "光照系统模式切换为: %s", auto_mode ? "AUTO" : "MANUAL");
}

bool isLightSystemAutoMode(void)
{
    return s_light_auto_mode;
}

void setAutoIntervalsSec(uint32_t feed_interval_sec, uint32_t clear_interval_sec)
{
    if (feed_interval_sec < AUTO_INTERVAL_MIN_SEC)
    {
        feed_interval_sec = AUTO_INTERVAL_MIN_SEC;
    }
    else if (feed_interval_sec > AUTO_INTERVAL_MAX_SEC)
    {
        feed_interval_sec = AUTO_INTERVAL_MAX_SEC;
    }

    if (clear_interval_sec < AUTO_INTERVAL_MIN_SEC)
    {
        clear_interval_sec = AUTO_INTERVAL_MIN_SEC;
    }
    else if (clear_interval_sec > AUTO_INTERVAL_MAX_SEC)
    {
        clear_interval_sec = AUTO_INTERVAL_MAX_SEC;
    }

    s_auto_feed_interval_ms = feed_interval_sec * 1000U;
    s_auto_clear_interval_ms = clear_interval_sec * 1000U;

    if (s_auto_feed_task_handle)
    {
        xTaskNotifyGive(s_auto_feed_task_handle);
    }

    if (s_auto_clear_task_handle)
    {
        xTaskNotifyGive(s_auto_clear_task_handle);
    }

    ESP_LOGI(TAG, "自动间隔已更新: Feed=%lus, Clear=%lus",
             (unsigned long)feed_interval_sec,
             (unsigned long)clear_interval_sec);
}

uint32_t getAutoFeedIntervalMs(void)
{
    return s_auto_feed_interval_ms;
}

uint32_t getAutoClearIntervalMs(void)
{
    return s_auto_clear_interval_ms;
}

void registerAutoFeedTaskHandle(TaskHandle_t task_handle)
{
    s_auto_feed_task_handle = task_handle;
}

void registerAutoClearTaskHandle(TaskHandle_t task_handle)
{
    s_auto_clear_task_handle = task_handle;
}

void control(float temperature, float humidity,
             float nh3_concentration, float h2s_concentration, int light)
{
    (void)humidity;

    uint32_t uptime_ms = pdTICKS_TO_MS(xTaskGetTickCount());
    bool gas_warmup_done = (uptime_ms >= GAS_WARMUP_MS);

    if (gas_warmup_done && !s_gas_warmup_done_logged)
    {
        ESP_LOGI(TAG, "气体传感器预热完成，已启用超阈值联动");
        s_gas_warmup_done_logged = true;
    }

    int fan_percent = 0;
    if (gas_warmup_done && (nh3_concentration > NH3_FAN_FULL_PPM || h2s_concentration > H2S_FAN_FULL_PPM))
    {
        fan_percent = 100;
        ESP_LOGW(TAG, "有毒气体超阈值，风扇满转: NH3=%.2fppm(阈值%.2f), H2S=%.2fppm(阈值%.2f)",
                 nh3_concentration, NH3_FAN_FULL_PPM, h2s_concentration, H2S_FAN_FULL_PPM);
    }
    else if (!gas_warmup_done)
    {
        if (!s_gas_warmup_skip_logged)
        {
            ESP_LOGI(TAG, "气体传感器预热中(60s)，暂不执行气体超阈值联动");
            s_gas_warmup_skip_logged = true;
        }

        if (temperature < 20.0f)
        {
            fan_percent = 10;
        }
        else if (temperature > 25.0f)
        {
            fan_percent = 100;
        }
        else
        {
            fan_percent = 10;
        }
    }
    else if (temperature < 20.0f)
    {
        fan_percent = 10;
    }
    else if (temperature > 25.0f)
    {
        fan_percent = 100;
    }
    else
    {
        fan_percent = 10;
    }

    if (fan_percent != s_last_fan_percent)
    {
        uint32_t duty = (uint32_t)(fan_percent * 255 / 100);
        ESP_ERROR_CHECK(pwm_set_duty(2, duty));
        tjc_printf("n0.val=%d", fan_percent);
        vTaskDelay(pdMS_TO_TICKS(20)); // 确保命令发送完成
        s_last_fan_percent = fan_percent;
    }

    if (!s_light_auto_mode)
    {
        return;
    }

    bool heat_on = (temperature < 20.0f);
    if (heat_on != s_last_heat_on)
    {
        apply_heat_state(heat_on, "auto_control");
    }

    // 光照控制顺序逻辑：
    // 1) 当前开灯且过强 -> 先关灯
    // 2) 下次仍过强 -> 关窗帘
    // 3) 关窗帘后若过暗 -> 再开灯
    bool light_on = s_last_light_on;

    if (light > LIGHT_THRESHOLD_HIGH)
    {
        if (s_last_light_on)
        {
            apply_light_state(false, "auto_high_light_step1_turn_off_light");
            s_wait_high_light_recheck = true;
            ESP_LOGI(TAG, "光照过强且当前开灯，先关灯后复测");
            return;
        }

        if (s_wait_high_light_recheck)
        {
            s_wait_high_light_recheck = false;
        }

        if (CurtainOpen)
        {
            apply_curtain_state(false, "auto_high_light_step2_close_curtain");
            s_curtain_closed_by_high_light = true;
            ESP_LOGI(TAG, "关灯后仍过强，关闭窗帘");
            return;
        }

        s_curtain_closed_by_high_light = true;
        light_on = false;
    }
    else if (light < LIGHT_THRESHOLD_LOW)
    {
        s_wait_high_light_recheck = false;

        if (s_curtain_closed_by_high_light && !CurtainOpen)
        {
            light_on = true;
            ESP_LOGI(TAG, "关窗帘后光照过暗，重新开启补光灯");
        }
        else if (!CurtainOpen)
        {
            apply_curtain_state(true, "auto_control_low_light");
            s_curtain_closed_by_high_light = false;
            light_on = false;
            ESP_LOGI(TAG, "光照过弱，打开窗帘增加光照");
        }
        else
        {
            light_on = true;
            ESP_LOGI(TAG, "光照过弱且窗帘已开，开启补光");
        }
    }
    else
    {
        s_wait_high_light_recheck = false;
        light_on = s_last_light_on;
    }

    if (light_on != s_last_light_on)
    {
        apply_light_state(light_on, "auto_control");
    }
}
void Opencurtain()
{
    sg90_set_angle_smooth(0, 180, 5, 40);
}
void Closecurtain()
{
    sg90_set_angle_smooth(0, 5, 5, 40);
}

void OpenLight()
{
}

static void apply_curtain_state(bool open, const char *reason)
{
    if (open != CurtainOpen)
    {
        if (open)
        {
            Opencurtain();
        }
        else
        {
            Closecurtain();
        }
        CurtainOpen = open;
        ESP_LOGI(TAG, "窗帘状态更新: %s, reason=%s", open ? "OPEN" : "CLOSE", reason ? reason : "N/A");
    }
}