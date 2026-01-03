// WS2812 (NeoPixel) 驱动实现（基于 RMT）
#include "gpio_driver.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "esp_err.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// 内部状态
static uint8_t *s_rgb_buf = NULL; // 存储为 RGB RGB ...
static int s_led_count = 0;
static gpio_num_t s_gpio = LED_STRIP_PIN;
static rmt_channel_t s_rmt_channel = RMT_CHANNEL_0;

// RMT 时钟分频（APB 80MHz / div），选用 2 => tick = 25ns
static const int RMT_CLK_DIV = 2;

// 按 tick (25ns) 的时序近似值
// 0: T0H ~0.4us, T0L ~0.85us
// 1: T1H ~0.8us, T1L ~0.45us
#define T0H_TICKS 16
#define T0L_TICKS 34
#define T1H_TICKS 32
#define T1L_TICKS 18
#define RESET_TICKS 2000 // 约 50us

// Helper: 释放并清空当前 buffer
static void _free_buf(void)
{
	if (s_rgb_buf) {
		free(s_rgb_buf);
		s_rgb_buf = NULL;
	}
	s_led_count = 0;
}

// 初始化 WS2812 驱动，gpio_num: 数据引脚，led_count: 灯珠数
esp_err_t neopixel_init(gpio_num_t gpio_num, int led_count)
{
	if (led_count <= 0) {
		return ESP_ERR_INVALID_ARG;
	}

	// 如果已初始化，先清理
	if (s_rgb_buf) {
		_free_buf();
		rmt_driver_uninstall(s_rmt_channel);
	}

	s_gpio = gpio_num;
	s_led_count = led_count;

	s_rgb_buf = (uint8_t *)malloc((size_t)s_led_count * 3);
	if (!s_rgb_buf) {
		// ESP_LOGE(TAG, "malloc failed");
		s_led_count = 0;
		return ESP_ERR_NO_MEM;
	}
	memset(s_rgb_buf, 0, (size_t)s_led_count * 3);

	rmt_config_t config = RMT_DEFAULT_CONFIG_TX(s_gpio, s_rmt_channel);
	config.clk_div = RMT_CLK_DIV;
	esp_err_t err = rmt_config(&config);
	if (err != ESP_OK) {
		// ESP_LOGE(TAG, "rmt_config failed: %d", err);
		_free_buf();
		return err;
	}

	err = rmt_driver_install(s_rmt_channel, 0, 0);
	if (err != ESP_OK) {
		// ESP_LOGE(TAG, "rmt_driver_install failed: %d", err);
		_free_buf();
		return err;
	}

	// ESP_LOGI(TAG, "neopixel init gpio=%d leds=%d", s_gpio, s_led_count);
	return ESP_OK;
}

// 设置单个像素的颜色（索引越界返回错误）
esp_err_t neopixel_set_pixel_rgb(int index, uint8_t red, uint8_t green, uint8_t blue)
{
	if (!s_rgb_buf || index < 0 || index >= s_led_count) return ESP_ERR_INVALID_ARG;

	// 应用亮度缩放
	uint8_t r = (uint8_t)((red * LED_STRIP_BRIGHTNESS) / 255);
	uint8_t g = (uint8_t)((green * LED_STRIP_BRIGHTNESS) / 255);
	uint8_t b = (uint8_t)((blue * LED_STRIP_BRIGHTNESS) / 255);

	int base = index * 3;
	s_rgb_buf[base + 0] = r;
	s_rgb_buf[base + 1] = g;
	s_rgb_buf[base + 2] = b;
	return ESP_OK;
}

// 直接设置整个缓冲区（按 RGB RGB ...）
esp_err_t neopixel_set_pixel_buf(const uint8_t *rgb_buffer, int buf_len)
{
	if (!s_rgb_buf || !rgb_buffer) return ESP_ERR_INVALID_ARG;
	if (buf_len != s_led_count * 3) return ESP_ERR_INVALID_SIZE;
	memcpy(s_rgb_buf, rgb_buffer, (size_t)buf_len);
	return ESP_OK;
}

// 将缓冲区内容发送到灯带（刷新显示）
esp_err_t neopixel_show(void)
{
	if (!s_rgb_buf || s_led_count <= 0) return ESP_ERR_INVALID_STATE;

	// 每个像素 24 位，每位对应一个 rmt_item
	int num_items = s_led_count * 24 + 1; // 末尾一个 reset
	rmt_item32_t *items = (rmt_item32_t *)malloc(sizeof(rmt_item32_t) * (size_t)num_items);
	if (!items) return ESP_ERR_NO_MEM;

	int item_idx = 0;
	for (int i = 0; i < s_led_count; i++) {
		// WS2812 要求按 GRB 顺序发送
		uint8_t r = s_rgb_buf[i*3 + 0];
		uint8_t g = s_rgb_buf[i*3 + 1];
		uint8_t b = s_rgb_buf[i*3 + 2];

		uint8_t colors[3] = {g, r, b};

		for (int c = 0; c < 3; c++) {
			for (int bit = 7; bit >= 0; bit--) {
				int bit_val = (colors[c] >> bit) & 0x01;
				rmt_item32_t item;
				if (bit_val) {
					item.level0 = 1;
					item.duration0 = T1H_TICKS;
					item.level1 = 0;
					item.duration1 = T1L_TICKS;
				} else {
					item.level0 = 1;
					item.duration0 = T0H_TICKS;
					item.level1 = 0;
					item.duration1 = T0L_TICKS;
				}
				items[item_idx++] = item;
			}
		}
	}

	// 追加一个低电平的 reset 脉冲 (>50us)
	rmt_item32_t reset_item;
	reset_item.level0 = 0;
	reset_item.duration0 = RESET_TICKS;
	reset_item.level1 = 0;
	reset_item.duration1 = 0;
	items[item_idx++] = reset_item;

	// 发送
	esp_err_t err = rmt_write_items(s_rmt_channel, items, item_idx, true);
	if (err != ESP_OK) {
		// ESP_LOGE(TAG, "rmt_write_items failed: %d", err);
		free(items);
		return err;
	}
	// 等待发送完成
	rmt_wait_tx_done(s_rmt_channel, portMAX_DELAY);

	free(items);
	return ESP_OK;
}

// 释放资源
void neopixel_deinit(void)
{
	_free_buf();
	rmt_driver_uninstall(s_rmt_channel);
}

// GPIO 初始化函数
void gpio_init_pin(gpio_num_t gpio_num, gpio_mode_t mode, bool pull_up_en, bool pull_down_en) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = mode,
        .pull_up_en = pull_up_en ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = pull_down_en ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

}

// RGB 循环变色
void RGB_Blink(void)
{
    static int state = 0;
    switch (state) {
        case 0:
            neopixel_set_pixel_rgb(0, 255, 0, 0); // Red
            break;
        case 1:
            neopixel_set_pixel_rgb(0, 0, 255, 0); // Green
            break;
        case 2:
            neopixel_set_pixel_rgb(0, 0, 0, 255); // Blue
            break;
    }
    neopixel_show();
    state = (state + 1) % 3;
}


    //写
    //gpio_set_level(gpio_num, level);

    //读
    //gpio_get_level(gpio_num);


