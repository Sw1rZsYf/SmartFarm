#ifndef LCD_ILI9341_H
#define LCD_ILI9341_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C"
{
#endif

// LCD尺寸定义
#define LCD_WIDTH 240
#define LCD_HEIGHT 320
#define LCD_PIXELS (LCD_WIDTH * LCD_HEIGHT)

// 硬件引脚配置
#define PIN_NUM_MISO -1 // 只写模式
#define PIN_NUM_MOSI 39 // ESP32S3 SPI2 MOSI
#define PIN_NUM_CLK 38  // ESP32S3 SPI2 CLK
#define PIN_NUM_CS 42   // ESP32S3 SPI2 CS

#define PIN_NUM_DC 40   // 数据/命令选择
#define PIN_NUM_RST 41  // 复位
#define PIN_NUM_BCKL 37 // 背光

#define LCD_HOST SPI2_HOST // 使用SPI2
#define LCD_BK_LIGHT_ON_LEVEL 1

// 性能优化参数
#define PARALLEL_LINES 32 // 每次传输32行，平衡内存和性能

    // LCD旋转方向
    typedef enum
    {
        LCD_ROTATION_0 = 0, // 0度，竖屏
        LCD_ROTATION_90,    // 90度，横屏
        LCD_ROTATION_180,   // 180度，竖屏翻转
        LCD_ROTATION_270    // 270度，横屏翻转
    } lcd_rotation_t;

// LCD颜色定义 (RGB565)
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_MAGENTA 0xF81F
#define COLOR_CYAN 0x07FF
#define COLOR_GRAY 0x8410

    /**
     * @brief 初始化LCD
     * @param spi_freq_hz SPI时钟频率（建议40MHz或80MHz）
     * @param rotation 屏幕旋转方向
     * @return esp_err_t 错误码
     */
    esp_err_t lcd_init(uint32_t spi_freq_hz, lcd_rotation_t rotation);

    /**
     * @brief 反初始化LCD，释放资源
     */
    void lcd_deinit(void);

    /**
     * @brief 设置显示区域（窗口）
     * @param x_start 起始X坐标
     * @param y_start 起始Y坐标
     * @param x_end 结束X坐标
     * @param y_end 结束Y坐标
     */
    void lcd_set_window(uint16_t x_start, uint16_t y_start,
                        uint16_t x_end, uint16_t y_end);

    /**
     * @brief 快速填充整个屏幕
     * @param color 16位颜色值
     */
    void lcd_fill_screen(uint16_t color);

    /**
     * @brief 填充矩形区域
     * @param x 起始X坐标
     * @param y 起始Y坐标
     * @param width 宽度
     * @param height 高度
     * @param color 颜色值
     */
    void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width,
                       uint16_t height, uint16_t color);

    /**
     * @brief 绘制单个像素点
     * @param x X坐标
     * @param y Y坐标
     * @param color 颜色值
     */
    void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

    /**
     * @brief 绘制水平线
     * @param x 起始X坐标
     * @param y Y坐标
     * @param length 线长度
     * @param color 颜色值
     */
    void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t length, uint16_t color);

    /**
     * @brief 绘制垂直线
     * @param x X坐标
     * @param y 起始Y坐标
     * @param length 线长度
     * @param color 颜色值
     */
    void lcd_draw_vline(uint16_t x, uint16_t y, uint16_t length, uint16_t color);

    /**
     * @brief 绘制矩形边框
     * @param x 起始X坐标
     * @param y 起始Y坐标
     * @param width 宽度
     * @param height 高度
     * @param color 颜色值
     */
    void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t width,
                       uint16_t height, uint16_t color);

    /**
     * @brief 绘制圆形
     * @param x0 圆心X坐标
     * @param y0 圆心Y坐标
     * @param radius 半径
     * @param color 颜色值
     */
    void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);

    /**
     * @brief 填充圆形
     * @param x0 圆心X坐标
     * @param y0 圆心Y坐标
     * @param radius 半径
     * @param color 颜色值
     */
    void lcd_fill_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color);

    /**
     * @brief 设置屏幕旋转方向
     * @param rotation 旋转方向
     */
    void lcd_set_rotation(lcd_rotation_t rotation);

    /**
     * @brief 设置背光亮度
     * @param brightness 亮度值 (0-100)
     */
    void lcd_set_brightness(uint8_t brightness);

    /**
     * @brief 获取当前屏幕宽度（考虑旋转）
     * @return uint16_t 屏幕宽度
     */
    uint16_t lcd_get_width(void);

    /**
     * @brief 获取当前屏幕高度（考虑旋转）
     * @return uint16_t 屏幕高度
     */
    uint16_t lcd_get_height(void);

    /**
     * @brief 显示测试图案
     */
    void lcd_show_test_pattern(void);

    /**
     * @brief 性能基准测试
     */
    void lcd_benchmark(void);

#ifdef __cplusplus
}
#endif

#endif // LCD_ILI9341_H