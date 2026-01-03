#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"

// NeoPixel配置
#define LED_STRIP_PIN 48
#define LED_STRIP_LED_NUM 1
#define LED_STRIP_BRIGHTNESS 30  // 亮度值 (0-255)

// 初始化 WS2812 驱动，gpio_num 可使用 GPIO_NUM_48
// led_count 为灯珠数量，调用一次即可；成功返回 ESP_OK
esp_err_t neopixel_init(gpio_num_t gpio_num, int led_count);

// 设置单个像素的颜色（0-based 索引），RGB 值范围 0-255
esp_err_t neopixel_set_pixel_rgb(int index, uint8_t red, uint8_t green, uint8_t blue);

// 直接提供 RGB 缓冲区（按 RGB RGB ... 顺序），长度应为 led_count*3
esp_err_t neopixel_set_pixel_buf(const uint8_t *rgb_buffer, int buf_len);

// 将当前缓冲区发送到灯带（刷新显示）
esp_err_t neopixel_show(void);

// 反初始化并释放资源
void neopixel_deinit(void);

// GPIO 初始化函数
// gpio_num: GPIO 引脚号
// mode: GPIO 模式（GPIO_MODE_INPUT 或 GPIO_MODE_OUTPUT）
// pull_up_en: 是否启用上拉（true/false）
// pull_down_en: 是否启用下拉（true/false）
void gpio_init_pin(gpio_num_t gpio_num, gpio_mode_t mode, bool pull_up_en, bool pull_down_en);

// RGB 循环变色函数
void RGB_Blink(void);



#endif