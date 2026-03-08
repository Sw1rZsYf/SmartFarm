/**
 * @file pwm_driver.h
 * @brief ESP32-S3 PWM输出驱动（基于LEDC）
 */

#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define PWM_PIN_0 GPIO_NUM_19
#define PWM_PIN_2 GPIO_NUM_13
    /**
     * @brief PWM通道配置结构体
     */
    typedef struct
    {
        uint8_t gpio_num;        /**< GPIO引脚号 */
        uint32_t freq_hz;        /**< PWM频率（Hz） */
        uint8_t duty_resolution; /**< 占空比分辨率（位），取值范围1-20，常用8-14位 */
        uint32_t duty_cycle;     /**< 初始占空比（0 ~ (2^duty_resolution - 1)） */
    } pwm_channel_config_t;

    /**
     * @brief 初始化PWM通道
     * @param channel 通道索引（0-7，取决于LEDC通道数）
     * @param config 通道配置
     * @return esp_err_t
     */
    esp_err_t pwm_init_channel(uint8_t channel, const pwm_channel_config_t *config);

    /**
     * @brief 设置PWM通道占空比
     * @param channel 通道索引
     * @param duty 占空比值（0 ~ (2^duty_resolution - 1)）
     * @return esp_err_t
     */
    esp_err_t pwm_set_duty(uint8_t channel, uint32_t duty);

    /**
     * @brief 设置PWM通道频率（将重新配置定时器）
     * @note 改变频率会影响同一定时器下的所有通道
     * @param channel 通道索引（用于定位定时器）
     * @param freq_hz 新频率（Hz）
     * @return esp_err_t
     */
    esp_err_t pwm_set_freq(uint8_t channel, uint32_t freq_hz);

    /**
     * @brief 启动PWM输出
     * @param channel 通道索引
     * @return esp_err_t
     */
    esp_err_t pwm_start(uint8_t channel);

    /**
     * @brief 停止PWM输出（输出低电平）
     * @param channel 通道索引
     * @return esp_err_t
     */
    esp_err_t pwm_stop(uint8_t channel);

    /**
     * @brief 反初始化PWM通道（释放资源）
     * @param channel 通道索引
     * @return esp_err_t
     */
    esp_err_t pwm_deinit_channel(uint8_t channel);

    void pwm_init(void);
    /**
     * @brief 让PWM占空比在指定时间内从起始值平滑变化到目标值（阻塞式）
     * @param channel 通道索引
     * @param start_duty 起始占空比
     * @param end_duty 目标占空比
     * @param fade_ms 渐变总时间（毫秒）
     * @return esp_err_t
     */
    esp_err_t pwm_fade_duty(uint8_t channel, uint32_t start_duty, uint32_t end_duty, uint32_t fade_ms);
    /**
     * @brief SG90舵机角度控制
     * @param channel PWM通道索引
     * @param angle 目标角度 (0-180度)
     * @return esp_err_t
     */
    esp_err_t sg90_set_angle(uint8_t channel, uint8_t angle);

    /**
     * @brief 平滑设置SG90舵机角度（从当前角度逐渐变化到目标角度）
     * @param channel PWM通道索引
     * @param target_angle 目标角度 (0-180)
     * @param step_deg 每次变化的角度步进值（建议1-5度）
     * @param step_delay_ms 每步之间的延时（毫秒）
     * @return esp_err_t
     */
    esp_err_t sg90_set_angle_smooth(uint8_t channel, uint8_t target_angle, uint8_t step_deg, uint32_t step_delay_ms);
#ifdef __cplusplus
}
#endif

#endif /* PWM_DRIVER_H */