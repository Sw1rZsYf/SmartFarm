#include "lcd_ili9341.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "string.h"
#include "math.h"
#include "esp_heap_caps.h"

static const char *TAG = "LCD_ILI9341";

// 全局变量
static spi_device_handle_t spi = NULL;
static bool spi_bus_initialized = false;
static lcd_rotation_t current_rotation = LCD_ROTATION_0;

// 静态缓冲区 - 避免频繁分配释放
static uint16_t *screen_buffer = NULL;

// 内存访问控制寄存器值（根据旋转方向）
static const uint8_t madctl_values[4] = {
    0x48, // 0度: 横屏，BGR，MY=0, MX=1, MV=0, ML=0, BGR=1, MH=0
    0x28, // 90度: 竖屏，BGR，MY=0, MX=0, MV=0, ML=0, BGR=1, MH=0
    0x88, // 180度: 横屏翻转，BGR
    0xE8  // 270度: 竖屏翻转，BGR
};

// SPI传输前回调函数
static void IRAM_ATTR lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

// 发送命令
static void lcd_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .user = (void *)0,
    };
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

// 发送数据
static void lcd_data(const uint8_t *data, size_t len)
{
    if (len == 0)
        return;

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .user = (void *)1,
    };
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

// 发送单字节数据
static void lcd_data_byte(uint8_t data)
{
    lcd_data(&data, 1);
}

// 发送16位数据
static void lcd_data_word(uint16_t data)
{
    uint8_t bytes[2] = {data >> 8, data & 0xFF};
    lcd_data(bytes, 2);
}

// 批量发送像素数据
static void lcd_send_pixels(const uint16_t *pixels, uint32_t count)
{
    if (count == 0)
        return;

    spi_transaction_t t = {
        .length = count * 16,
        .tx_buffer = pixels,
        .user = (void *)1,
    };

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

// 初始化缓冲区
static esp_err_t lcd_init_buffer(void)
{
    if (screen_buffer != NULL)
    {
        free(screen_buffer);
    }

    // 使用DMA内存，提高传输效率
    screen_buffer = (uint16_t *)heap_caps_malloc(
        LCD_WIDTH * PARALLEL_LINES * sizeof(uint16_t),
        MALLOC_CAP_DMA);

    if (screen_buffer == NULL)
    {
        ESP_LOGE(TAG, "缓冲区分配失败");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "缓冲区分配成功，大小: %lu字节",
             LCD_WIDTH * PARALLEL_LINES * sizeof(uint16_t));
    return ESP_OK;
}

// 硬件复位
static void lcd_hardware_reset(void)
{
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

// ILI9341初始化序列
static void lcd_init_sequence(void)
{
    ESP_LOGI(TAG, "开始ILI9341初始化...");

    // 软件复位
    lcd_cmd(0x01);
    vTaskDelay(120 / portTICK_PERIOD_MS);

    // 电源控制B
    lcd_cmd(0xCF);
    lcd_data_byte(0x00);
    lcd_data_byte(0x83);
    lcd_data_byte(0x30);

    // 电源控制序列
    lcd_cmd(0xED);
    lcd_data_byte(0x64);
    lcd_data_byte(0x03);
    lcd_data_byte(0x12);
    lcd_data_byte(0x81);

    // 驱动器时序控制A
    lcd_cmd(0xE8);
    lcd_data_byte(0x85);
    lcd_data_byte(0x01);
    lcd_data_byte(0x79);

    // 电源控制A
    lcd_cmd(0xCB);
    lcd_data_byte(0x39);
    lcd_data_byte(0x2C);
    lcd_data_byte(0x00);
    lcd_data_byte(0x34);
    lcd_data_byte(0x02);

    // 泵比例控制
    lcd_cmd(0xF7);
    lcd_data_byte(0x20);

    // 电源控制1
    lcd_cmd(0xC0);
    lcd_data_byte(0x26); // GVDD = 4.75V

    // 电源控制2
    lcd_cmd(0xC1);
    lcd_data_byte(0x11); // VGH = VCI*2, VGL = -VCI*2

    // VCOM控制1
    lcd_cmd(0xC5);
    lcd_data_byte(0x35);
    lcd_data_byte(0x3E);

    // VCOM控制2
    lcd_cmd(0xC7);
    lcd_data_byte(0xBE);

    // 内存访问控制（根据旋转方向）
    lcd_cmd(0x36);
    lcd_data_byte(madctl_values[current_rotation]);

    // 像素格式
    lcd_cmd(0x3A);
    lcd_data_byte(0x55); // 16位/像素

    // 帧率控制（60Hz）
    lcd_cmd(0xB1);
    lcd_data_byte(0x00);
    lcd_data_byte(0x1B);

    // 显示功能控制
    lcd_cmd(0xB6);
    lcd_data_byte(0x0A);
    lcd_data_byte(0xA2);

    // 3G控制
    lcd_cmd(0xF2);
    lcd_data_byte(0x08);

    // Gamma设置
    lcd_cmd(0x26);
    lcd_data_byte(0x01); // Gamma曲线1

    // 正极性伽马校正
    lcd_cmd(0xE0);
    uint8_t pos_gamma[] = {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87,
                           0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00};
    lcd_data(pos_gamma, sizeof(pos_gamma));

    // 负极性伽马校正
    lcd_cmd(0xE1);
    uint8_t neg_gamma[] = {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78,
                           0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F};
    lcd_data(neg_gamma, sizeof(neg_gamma));

    // 退出睡眠模式
    lcd_cmd(0x11);
    vTaskDelay(120 / portTICK_PERIOD_MS);

    // 开启显示
    lcd_cmd(0x29);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "ILI9341初始化完成");
}

// 配置GPIO
static esp_err_t lcd_init_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_BCKL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO配置失败");
        return ret;
    }

    return ESP_OK;
}

// 初始化SPI总线
static esp_err_t lcd_init_spi_bus(uint32_t spi_freq_hz)
{
    if (spi_bus_initialized)
    {
        return ESP_OK;
    }

    // SPI总线配置
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * PARALLEL_LINES * 2 + 8,
    };

    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI总线初始化失败");
        return ret;
    }

    spi_bus_initialized = true;
    return ESP_OK;
}

// 添加SPI设备
static esp_err_t lcd_add_spi_device(uint32_t spi_freq_hz)
{
    // 如果设备已存在，先移除
    if (spi != NULL)
    {
        spi_bus_remove_device(spi);
        spi = NULL;
    }

    // SPI设备配置
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = spi_freq_hz,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 1,
        .pre_cb = lcd_spi_pre_transfer_callback,
    };

    esp_err_t ret = spi_bus_add_device(LCD_HOST, &devcfg, &spi);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI设备添加失败");
        return ret;
    }

    return ESP_OK;
}

// 公共API实现
esp_err_t lcd_init(uint32_t spi_freq_hz, lcd_rotation_t rotation)
{
    esp_err_t ret;

    current_rotation = rotation;

    // 1. 配置GPIO
    ret = lcd_init_gpio();
    if (ret != ESP_OK)
        return ret;

    // 2. 硬件复位
    lcd_hardware_reset();

    // 3. 初始化SPI总线
    ret = lcd_init_spi_bus(spi_freq_hz);
    if (ret != ESP_OK)
        return ret;

    // 4. 添加SPI设备
    ret = lcd_add_spi_device(spi_freq_hz);
    if (ret != ESP_OK)
        return ret;

    // 5. 初始化缓冲区
    ret = lcd_init_buffer();
    if (ret != ESP_OK)
        return ret;

    // 6. 初始化LCD序列
    lcd_init_sequence();

    // 7. 开启背光
    gpio_set_level(PIN_NUM_BCKL, LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "LCD初始化完成，频率: %luHz, 旋转: %d", spi_freq_hz, rotation);
    return ESP_OK;
}

void lcd_deinit(void)
{
    if (screen_buffer != NULL)
    {
        free(screen_buffer);
        screen_buffer = NULL;
    }

    if (spi != NULL)
    {
        spi_bus_remove_device(spi);
        spi = NULL;
    }

    if (spi_bus_initialized)
    {
        spi_bus_free(LCD_HOST);
        spi_bus_initialized = false;
    }

    ESP_LOGI(TAG, "LCD已去初始化");
}

void lcd_set_window(uint16_t x_start, uint16_t y_start,
                    uint16_t x_end, uint16_t y_end)
{
    // 设置列地址
    lcd_cmd(0x2A);
    lcd_data_byte(x_start >> 8);
    lcd_data_byte(x_start & 0xFF);
    lcd_data_byte(x_end >> 8);
    lcd_data_byte(x_end & 0xFF);

    // 设置行地址
    lcd_cmd(0x2B);
    lcd_data_byte(y_start >> 8);
    lcd_data_byte(y_start & 0xFF);
    lcd_data_byte(y_end >> 8);
    lcd_data_byte(y_end & 0xFF);

    // 写入内存命令
    lcd_cmd(0x2C);
}

void lcd_fill_screen(uint16_t color)
{
    if (screen_buffer == NULL)
    {
        ESP_LOGE(TAG, "缓冲区未初始化");
        return;
    }

    // 填充缓冲区
    for (int i = 0; i < LCD_WIDTH * PARALLEL_LINES; i++)
    {
        screen_buffer[i] = color;
    }

    // 设置窗口为整个屏幕
    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    // 每次发送PARALLEL_LINES行
    for (int y = 0; y < LCD_HEIGHT; y += PARALLEL_LINES)
    {
        int lines_to_send = (y + PARALLEL_LINES > LCD_HEIGHT) ? (LCD_HEIGHT - y) : PARALLEL_LINES;
        lcd_send_pixels(screen_buffer, LCD_WIDTH * lines_to_send);
    }
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width,
                   uint16_t height, uint16_t color)
{
    if (screen_buffer == NULL)
    {
        ESP_LOGE(TAG, "缓冲区未初始化");
        return;
    }

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;

    uint16_t x_end = x + width - 1;
    uint16_t y_end = y + height - 1;

    if (x_end >= LCD_WIDTH)
        x_end = LCD_WIDTH - 1;
    if (y_end >= LCD_HEIGHT)
        y_end = LCD_HEIGHT - 1;

    uint16_t actual_width = x_end - x + 1;
    uint16_t actual_height = y_end - y + 1;

    // 填充缓冲区的一行数据
    if (actual_width <= LCD_WIDTH * PARALLEL_LINES)
    {
        for (int i = 0; i < actual_width; i++)
        {
            screen_buffer[i] = color;
        }

        // 设置窗口
        lcd_set_window(x, y, x_end, y_end);

        // 发送所有行
        for (int row = 0; row < actual_height; row++)
        {
            lcd_send_pixels(screen_buffer, actual_width);
        }
    }
    else
    {
        ESP_LOGE(TAG, "矩形宽度超过缓冲区大小");
    }
}

// 其他函数保持不变（略，只显示修改部分）

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;

    lcd_set_window(x, y, x, y);
    lcd_data_word(color);
}

void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t length, uint16_t color)
{
    if (y >= LCD_HEIGHT)
        return;

    uint16_t x_end = x + length - 1;
    if (x_end >= LCD_WIDTH)
        x_end = LCD_WIDTH - 1;

    lcd_fill_rect(x, y, x_end - x + 1, 1, color);
}

void lcd_draw_vline(uint16_t x, uint16_t y, uint16_t length, uint16_t color)
{
    if (x >= LCD_WIDTH)
        return;

    uint16_t y_end = y + length - 1;
    if (y_end >= LCD_HEIGHT)
        y_end = LCD_HEIGHT - 1;

    lcd_fill_rect(x, y, 1, y_end - y + 1, color);
}

void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t width,
                   uint16_t height, uint16_t color)
{
    // 绘制四条边
    lcd_draw_hline(x, y, width, color);              // 上边
    lcd_draw_hline(x, y + height - 1, width, color); // 下边
    lcd_draw_vline(x, y, height, color);             // 左边
    lcd_draw_vline(x + width - 1, y, height, color); // 右边
}

void lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y)
    {
        lcd_draw_pixel(x0 + x, y0 + y, color);
        lcd_draw_pixel(x0 + y, y0 + x, color);
        lcd_draw_pixel(x0 - y, y0 + x, color);
        lcd_draw_pixel(x0 - x, y0 + y, color);
        lcd_draw_pixel(x0 - x, y0 - y, color);
        lcd_draw_pixel(x0 - y, y0 - x, color);
        lcd_draw_pixel(x0 + y, y0 - x, color);
        lcd_draw_pixel(x0 + x, y0 - y, color);

        if (err <= 0)
        {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void lcd_fill_circle(uint16_t x0, uint16_t y0, uint16_t radius, uint16_t color)
{
    int x = radius;
    int y = 0;
    int err = 0;
    int width = 0;

    while (x >= y)
    {
        width = x;

        // 绘制水平线填充圆
        lcd_draw_hline(x0 - width, y0 + y, 2 * width, color);
        lcd_draw_hline(x0 - width, y0 - y, 2 * width, color);

        width = y;
        lcd_draw_hline(x0 - width, y0 + x, 2 * width, color);
        lcd_draw_hline(x0 - width, y0 - x, 2 * width, color);

        if (err <= 0)
        {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void lcd_set_rotation(lcd_rotation_t rotation)
{
    if (rotation >= 4)
        return;

    current_rotation = rotation;

    // 更新内存访问控制寄存器
    lcd_cmd(0x36);
    lcd_data_byte(madctl_values[rotation]);
}

void lcd_set_brightness(uint8_t brightness)
{
    // 简化实现：只支持开关背光
    if (brightness > 0)
    {
        gpio_set_level(PIN_NUM_BCKL, LCD_BK_LIGHT_ON_LEVEL);
    }
    else
    {
        gpio_set_level(PIN_NUM_BCKL, 0);
    }
}

uint16_t lcd_get_width(void)
{
    // 根据旋转方向返回宽度
    if (current_rotation == LCD_ROTATION_0 || current_rotation == LCD_ROTATION_180)
    {
        return LCD_WIDTH;
    }
    else
    {
        return LCD_HEIGHT;
    }
}

uint16_t lcd_get_height(void)
{
    // 根据旋转方向返回高度
    if (current_rotation == LCD_ROTATION_0 || current_rotation == LCD_ROTATION_180)
    {
        return LCD_HEIGHT;
    }
    else
    {
        return LCD_WIDTH;
    }
}

void lcd_show_test_pattern(void)
{
    ESP_LOGI(TAG, "显示测试图案...");

    // 1. 清屏黑色
    lcd_fill_screen(COLOR_BLACK);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // 2. 显示纯色
    lcd_fill_screen(COLOR_RED);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    lcd_fill_screen(COLOR_GREEN);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    lcd_fill_screen(COLOR_BLUE);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    lcd_fill_screen(COLOR_WHITE);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // 3. 彩色网格
    lcd_fill_screen(COLOR_BLACK);

    // 画网格线
    for (int x = 0; x < LCD_WIDTH; x += 20)
    {
        lcd_draw_vline(x, 0, LCD_HEIGHT, COLOR_GRAY);
    }
    for (int y = 0; y < LCD_HEIGHT; y += 20)
    {
        lcd_draw_hline(0, y, LCD_WIDTH, COLOR_GRAY);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // 4. 几何图形
    lcd_fill_screen(COLOR_BLACK);

    // 绘制多个圆形
    for (int i = 1; i <= 4; i++)
    {
        lcd_draw_circle(LCD_WIDTH / 2, LCD_HEIGHT / 2, i * 20, COLOR_RED);
        lcd_draw_circle(LCD_WIDTH / 4, LCD_HEIGHT / 4, i * 15, COLOR_GREEN);
        lcd_draw_circle(LCD_WIDTH * 3 / 4, LCD_HEIGHT * 3 / 4, i * 15, COLOR_BLUE);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // 5. 填充矩形
    lcd_fill_screen(COLOR_BLACK);

    lcd_fill_rect(50, 50, 100, 50, COLOR_RED);
    lcd_fill_rect(170, 50, 100, 50, COLOR_GREEN);
    lcd_fill_rect(50, 140, 100, 50, COLOR_BLUE);
    lcd_fill_rect(170, 140, 100, 50, COLOR_YELLOW);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "测试图案显示完成");
}

void lcd_benchmark(void)
{
    uint32_t start_time, end_time;

    ESP_LOGI(TAG, "开始性能基准测试...");

    // 测试1: 全屏填充
    start_time = esp_log_timestamp();
    lcd_fill_screen(COLOR_RED);
    lcd_fill_screen(COLOR_GREEN);
    lcd_fill_screen(COLOR_BLUE);
    end_time = esp_log_timestamp();

    uint32_t fill_time = (end_time - start_time) / 3; // 平均单次填充时间
    float fps = 1000.0 / fill_time;                   // 理论最大FPS

    ESP_LOGI(TAG, "全屏填充时间: %lums, 理论FPS: %.1f", fill_time, fps);

    // 测试2: 矩形填充
    start_time = esp_log_timestamp();
    for (int i = 0; i < 100; i++)
    {
        uint16_t x = rand() % (LCD_WIDTH - 50);
        uint16_t y = rand() % (LCD_HEIGHT - 50);
        uint16_t color = rand() & 0xFFFF;
        lcd_fill_rect(x, y, 50, 50, color);
    }
    end_time = esp_log_timestamp();

    ESP_LOGI(TAG, "100个矩形填充时间: %lums", end_time - start_time);

    // 测试3: 像素绘制
    start_time = esp_log_timestamp();
    for (int i = 0; i < 1000; i++)
    {
        uint16_t x = rand() % LCD_WIDTH;
        uint16_t y = rand() % LCD_HEIGHT;
        uint16_t color = rand() & 0xFFFF;
        lcd_draw_pixel(x, y, color);
    }
    end_time = esp_log_timestamp();

    ESP_LOGI(TAG, "1000个像素绘制时间: %lums", end_time - start_time);

    ESP_LOGI(TAG, "性能基准测试完成");
}