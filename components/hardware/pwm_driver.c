/**
 * @file pwm_driver.c
 * @brief ESP32-S3 PWM输出驱动实现
 */

#include <stdio.h>
#include <string.h>
#include "pwm_driver.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "PWM_DRIVER";

// 最大通道数（ESP32-S3 LEDC共有8个通道）
#define PWM_MAX_CHANNELS 8

// 通道状态记录
typedef struct
{
    bool is_initialized;              // 是否已初始化
    ledc_channel_t ledc_channel;      // LEDC通道号
    ledc_timer_t ledc_timer;          // 使用的定时器
    ledc_timer_bit_t duty_resolution; // 占空比分辨率
    uint32_t freq_hz;                 // 当前频率
} pwm_channel_state_t;

static pwm_channel_state_t s_channels[PWM_MAX_CHANNELS] = {0};

// 辅助函数：根据频率和分辨率选择合适的定时器时钟源和分频系数
static esp_err_t pwm_timer_config(ledc_timer_t timer, uint32_t freq_hz, ledc_timer_bit_t duty_res)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE, // ESP32-S3仅支持低速模式
        .timer_num = timer,
        .duty_resolution = duty_res,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK // 自动选择时钟源
    };
    return ledc_timer_config(&timer_conf);
}

esp_err_t pwm_init_channel(uint8_t channel, const pwm_channel_config_t *config)
{
    if (channel >= PWM_MAX_CHANNELS)
    {
        ESP_LOGE(TAG, "无效通道号: %d (最大 %d)", channel, PWM_MAX_CHANNELS - 1);
        return ESP_ERR_INVALID_ARG;
    }
    if (s_channels[channel].is_initialized)
    {
        ESP_LOGW(TAG, "通道 %d 已初始化，将重新配置", channel);
        // 先释放
        pwm_deinit_channel(channel);
    }

    // 参数检查
    if (config->duty_resolution < 1 || config->duty_resolution > 20)
    {
        ESP_LOGE(TAG, "占空比分辨率 %d 超出范围(1-20)", config->duty_resolution);
        return ESP_ERR_INVALID_ARG;
    }

    // 分配LEDC通道和定时器（简单起见：通道号直接映射，定时器按通道号/2分配，让每个定时器带两个通道）
    ledc_channel_t ledc_chan = (ledc_channel_t)(channel % 8);
    ledc_timer_t ledc_timer = (ledc_timer_t)(channel / 2); // 0,0,1,1,2,2,3,3

    // 配置定时器
    ledc_timer_bit_t resolution = (ledc_timer_bit_t)(config->duty_resolution);
    esp_err_t ret = pwm_timer_config(ledc_timer, config->freq_hz, resolution);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "定时器配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置通道
    ledc_channel_config_t chan_conf = {
        .gpio_num = config->gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = ledc_chan,
        .timer_sel = ledc_timer,
        .duty = config->duty_cycle,
        .hpoint = 0,
        .flags.output_invert = 0};
    ret = ledc_channel_config(&chan_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "通道配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 保存状态
    s_channels[channel].is_initialized = true;
    s_channels[channel].ledc_channel = ledc_chan;
    s_channels[channel].ledc_timer = ledc_timer;
    s_channels[channel].duty_resolution = resolution;
    s_channels[channel].freq_hz = config->freq_hz;

    ESP_LOGI(TAG, "通道 %d 初始化成功 (GPIO%d, 频率 %d Hz, 分辨率 %d 位)",
             channel, config->gpio_num, config->freq_hz, config->duty_resolution);
    return ESP_OK;
}

esp_err_t pwm_set_duty(uint8_t channel, uint32_t duty)
{
    if (channel >= PWM_MAX_CHANNELS || !s_channels[channel].is_initialized)
    {
        ESP_LOGE(TAG, "通道 %d 未初始化", channel);
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t max_duty = (1 << s_channels[channel].duty_resolution) - 1;
    if (duty > max_duty)
    {
        ESP_LOGW(TAG, "占空比 %d 超出范围(0-%d)，将被截断", duty, max_duty);
        duty = max_duty;
    }

    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, s_channels[channel].ledc_channel, duty);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "设置占空比失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, s_channels[channel].ledc_channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "更新占空比失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "通道 %d 占空比设置为 %d", channel, duty);
    return ESP_OK;
}

esp_err_t pwm_set_freq(uint8_t channel, uint32_t freq_hz)
{
    if (channel >= PWM_MAX_CHANNELS || !s_channels[channel].is_initialized)
    {
        ESP_LOGE(TAG, "通道 %d 未初始化", channel);
        return ESP_ERR_INVALID_STATE;
    }

    ledc_timer_t timer = s_channels[channel].ledc_timer;
    ledc_timer_bit_t resolution = s_channels[channel].duty_resolution;

    esp_err_t ret = pwm_timer_config(timer, freq_hz, resolution);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "重新配置定时器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 更新记录的频率
    s_channels[channel].freq_hz = freq_hz;

    ESP_LOGI(TAG, "通道 %d 频率设置为 %d Hz", channel, freq_hz);
    return ESP_OK;
}

esp_err_t pwm_start(uint8_t channel)
{
    if (channel >= PWM_MAX_CHANNELS || !s_channels[channel].is_initialized)
    {
        ESP_LOGE(TAG, "通道 %d 未初始化", channel);
        return ESP_ERR_INVALID_STATE;
    }

    // LEDC通道使能输出，只需更新占空比即可启动
    // 如果需要强制输出低电平后恢复，可先设置占空比为0再恢复，但这里简单处理：确保占空比已更新
    esp_err_t ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, s_channels[channel].ledc_channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "启动通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "通道 %d 已启动", channel);
    return ESP_OK;
}

esp_err_t pwm_stop(uint8_t channel)
{
    if (channel >= PWM_MAX_CHANNELS || !s_channels[channel].is_initialized)
    {
        ESP_LOGE(TAG, "通道 %d 未初始化", channel);
        return ESP_ERR_INVALID_STATE;
    }

    // 将占空比设为0（输出低电平）
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, s_channels[channel].ledc_channel, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "停止通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, s_channels[channel].ledc_channel);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "更新占空比失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "通道 %d 已停止", channel);
    return ESP_OK;
}

esp_err_t pwm_deinit_channel(uint8_t channel)
{
    if (channel >= PWM_MAX_CHANNELS)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_channels[channel].is_initialized)
    {
        ESP_LOGW(TAG, "通道 %d 未初始化，无需反初始化", channel);
        return ESP_OK;
    }

    // 停止输出
    pwm_stop(channel);

    // 释放GPIO（可选：将引脚设置为悬空输入）
    // 注意：LEDC驱动没有直接的反初始化函数，我们可以重置通道状态
    s_channels[channel].is_initialized = false;

    ESP_LOGI(TAG, "通道 %d 已反初始化", channel);
    return ESP_OK;
}

void pwm_init(void)
{
    // 舵机通道0 (定时器0)
    pwm_channel_config_t ch0_config = {
        .gpio_num = PWM_PIN_0, // 舵机GPIO
        .freq_hz = 50,
        .duty_resolution = 12,
        .duty_cycle = 0};
    ESP_ERROR_CHECK(pwm_init_channel(0, &ch0_config));

    // 风扇通道2 (定时器1)
    pwm_channel_config_t ch2_config = {
        .gpio_num = PWM_PIN_2, // 风扇GPIO（需重新指定引脚）
        .freq_hz = 25000,      // 25kHz
        .duty_resolution = 8,
        .duty_cycle = 0};
    ESP_ERROR_CHECK(pwm_init_channel(2, &ch2_config));
}

esp_err_t pwm_fade_duty(uint8_t channel, uint32_t start_duty, uint32_t end_duty, uint32_t fade_ms)
{
    if (channel >= PWM_MAX_CHANNELS || !s_channels[channel].is_initialized)
    {
        ESP_LOGE(TAG, "通道 %d 未初始化", channel);
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t max_duty = (1 << s_channels[channel].duty_resolution) - 1;
    if (start_duty > max_duty || end_duty > max_duty)
    {
        ESP_LOGE(TAG, "占空比超出范围(0-%d)", max_duty);
        return ESP_ERR_INVALID_ARG;
    }

    if (start_duty == end_duty)
    {
        // 无需变化
        return pwm_set_duty(channel, end_duty);
    }

    // 渐变步进间隔（毫秒），可根据需要调整
    const uint32_t step_ms = 20;
    uint32_t total_steps = fade_ms / step_ms;
    if (total_steps == 0)
    {
        // 渐变时间太短，直接设置目标值
        return pwm_set_duty(channel, end_duty);
    }

    int32_t delta = (int32_t)end_duty - (int32_t)start_duty;
    uint32_t current_duty = start_duty;

    // 先设置起始占空比
    esp_err_t ret = pwm_set_duty(channel, start_duty);
    if (ret != ESP_OK)
    {
        return ret;
    }

    for (uint32_t step = 1; step <= total_steps; step++)
    {
        // 计算当前步的占空比（线性插值）
        uint32_t target_duty = start_duty + (delta * step) / total_steps;
        if (target_duty > max_duty)
        {
            target_duty = max_duty;
        }
        ret = pwm_set_duty(channel, target_duty);
        if (ret != ESP_OK)
        {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }

    // 确保最终占空比精确为目标值
    ret = pwm_set_duty(channel, end_duty);
    return ret;
}

/**
 * @brief SG90舵机角度控制
 *
 * SG90舵机控制原理[citation:1][citation:6]：
 * - PWM频率：50Hz（周期20ms）
 * - 0.5ms 高电平 → 0度（占空比 2.5%）
 * - 1.5ms 高电平 → 90度（占空比 7.5%）
 * - 2.5ms 高电平 → 180度（占空比 12.5%）
 *
 * 占空比计算公式（基于10位分辨率PWM）：
 * duty = (angle * (125-25) / 180) + 25
 * 其中25对应0.5ms（2.5% * 1024 ≈ 25.6 → 25）
 * 125对应2.5ms（12.5% * 1024 = 128 → 125，取整处理）
 */
esp_err_t sg90_set_angle(uint8_t channel, uint8_t angle)
{
    if (channel >= PWM_MAX_CHANNELS || !s_channels[channel].is_initialized)
    {
        ESP_LOGE(TAG, "通道 %d 未初始化", channel);
        return ESP_ERR_INVALID_STATE;
    }

    // 角度参数检查
    if (angle > 180)
    {
        ESP_LOGW(TAG, "角度 %d 超出范围(0-180)，将被限制为180", angle);
        angle = 180;
    }

    // 获取当前分辨率的最大占空比
    uint32_t max_duty = (1 << s_channels[channel].duty_resolution) - 1;

    // 计算对应角度的占空比
    // 公式说明：
    // 0度对应占空比：0.5ms/20ms = 2.5% → duty_min = max_duty * 2.5%
    // 180度对应占空比：2.5ms/20ms = 12.5% → duty_max = max_duty * 12.5%
    // 中间角度线性插值

    // 以10位分辨率（max_duty=1023）为例：
    // 0度：1023 * 2.5% ≈ 25.6 → 25
    // 180度：1023 * 12.5% ≈ 127.9 → 125（取整处理）
    uint32_t duty_min = max_duty * 25 / 1000;  // 2.5%
    uint32_t duty_max = max_duty * 125 / 1000; // 12.5%

    // 线性插值计算目标占空比
    uint32_t duty = duty_min + (angle * (duty_max - duty_min) / 180);

    ESP_LOGD(TAG, "舵机通道 %d 设置角度 %d° -> 占空比 %d (范围: %d-%d)",
             channel, angle, duty, duty_min, duty_max);

    return pwm_set_duty(channel, duty);
}

esp_err_t sg90_set_angle_smooth(uint8_t channel, uint8_t target_angle, uint8_t step_deg, uint32_t step_delay_ms)
{
    if (channel >= PWM_MAX_CHANNELS || !s_channels[channel].is_initialized)
    {
        ESP_LOGE(TAG, "通道 %d 未初始化", channel);
        return ESP_ERR_INVALID_STATE;
    }

    if (target_angle > 180)
    {
        ESP_LOGW(TAG, "目标角度 %d 超出范围，限制为180", target_angle);
        target_angle = 180;
    }

    if (step_deg == 0)
    {
        step_deg = 1; // 防止死循环
    }

    // 获取当前占空比
    uint32_t current_duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, s_channels[channel].ledc_channel);

    // 计算当前角度
    uint32_t max_duty = (1 << s_channels[channel].duty_resolution) - 1;
    uint32_t duty_min = max_duty * 25 / 1000;  // 0.5ms对应2.5%
    uint32_t duty_max = max_duty * 125 / 1000; // 2.5ms对应12.5%

    // 限制当前占空比在合理范围内
    if (current_duty < duty_min)
        current_duty = duty_min;
    if (current_duty > duty_max)
        current_duty = duty_max;

    // 将当前占空比转换为角度（线性映射）
    uint8_t current_angle = (current_duty - duty_min) * 180 / (duty_max - duty_min);

    if (current_angle == target_angle)
    {
        return ESP_OK; // 已在目标角度
    }

    // 确定步进方向
    int8_t direction = (current_angle < target_angle) ? 1 : -1;
    uint8_t angle = current_angle;

    // 逐步调整角度
    while (1)
    {
        // 计算下一步的角度
        if (direction > 0)
        {
            angle += step_deg;
            if (angle > target_angle)
                angle = target_angle;
        }
        else
        {
            if (angle < step_deg)
            {
                angle = 0;
            }
            else
            {
                angle -= step_deg;
            }
            if (angle < target_angle)
                angle = target_angle;
        }

        // 设置角度
        esp_err_t ret = sg90_set_angle(channel, angle);
        if (ret != ESP_OK)
        {
            return ret;
        }

        if (angle == target_angle)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    }

    return ESP_OK;
}