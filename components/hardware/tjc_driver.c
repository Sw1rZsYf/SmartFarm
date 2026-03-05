// /**
//  * @file tjc_driver.c
//  * @brief 淘晶驰T1系列串口屏驱动程序 for ESP32-S3 (ESP-IDF v5.2)
//  *
//  * 硬件连接说明：
//  * - 串口屏 TXD <--> ESP32-S3 GPIO4 (RX)
//  * - 串口屏 RXD <--> ESP32-S3 GPIO5 (TX)
//  * - 串口屏 GND <--> ESP32-S3 GND
//  * - 串口屏 5V  <--> ESP32-S3 5V (或外部5V电源)
//  *
//  * 引脚定义基于淘晶驰T1系列FPC 1.0*14接口规范 [citation:2]
//  */

// #include <stdio.h>
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "driver/uart.h"
// #include "esp_log.h"
// #include "esp_err.h"

// // 配置标签
// static const char *TAG = "TJC_DRIVER";

// // UART配置参数
// #define TJC_UART_PORT_NUM UART_NUM_2 // 使用UART2（避免与调试串口UART0冲突）
// #define TJC_UART_BAUD_RATE 115200    // 淘晶驰串口屏默认波特率 [citation:2]
// #define TJC_UART_TX_GPIO 5           // ESP32-S3 TX -> 串口屏 RX
// #define TJC_UART_RX_GPIO 4           // ESP32-S3 RX -> 串口屏 TX
// #define TJC_UART_RTS_GPIO UART_PIN_NO_CHANGE
// #define TJC_UART_CTS_GPIO UART_PIN_NO_CHANGE

// // 缓冲区大小
// #define TJC_UART_BUF_SIZE (1024 * 2) // 2K缓冲区
// #define TJC_CMD_MAX_LEN 256          // 最大指令长度

// // 指令结束符（淘晶驰协议：每条指令以三个0xFF结尾）[citation:5]
// #define TJC_CMD_END1 0xFF
// #define TJC_CMD_END2 0xFF
// #define TJC_CMD_END3 0xFF

// /**
//  * @brief 初始化串口
//  * @return esp_err_t ESP_OK表示成功
//  */
// esp_err_t tjc_uart_init(void)
// {
//     ESP_LOGI(TAG, "初始化串口 - 波特率: %d, TX:%d, RX:%d",
//              TJC_UART_BAUD_RATE, TJC_UART_TX_GPIO, TJC_UART_RX_GPIO);

//     // 1. 配置UART参数 [citation:4]
//     uart_config_t uart_config = {
//         .baud_rate = TJC_UART_BAUD_RATE,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT, // ESP-IDF v5.2 使用默认时钟源
//     };

//     // 应用UART参数
//     esp_err_t ret = uart_param_config(TJC_UART_PORT_NUM, &uart_config);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "UART参数配置失败: %s", esp_err_to_name(ret));
//         return ret;
//     }

//     // 2. 设置UART引脚 [citation:2]
//     ret = uart_set_pin(TJC_UART_PORT_NUM,
//                        TJC_UART_TX_GPIO,   // TX引脚
//                        TJC_UART_RX_GPIO,   // RX引脚
//                        TJC_UART_RTS_GPIO,  // RTS (不使用)
//                        TJC_UART_CTS_GPIO); // CTS (不使用)
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "UART引脚配置失败: %s", esp_err_to_name(ret));
//         return ret;
//     }

//     // 3. 安装UART驱动 [citation:4]
//     // 参数: 端口号, 接收缓冲区大小, 发送缓冲区大小, 事件队列大小, 事件队列句柄, 分配标志
//     ret = uart_driver_install(TJC_UART_PORT_NUM,
//                               TJC_UART_BUF_SIZE, // 接收缓冲区
//                               TJC_UART_BUF_SIZE, // 发送缓冲区
//                               0,                 // 不使用事件队列
//                               NULL,              // 不获取队列句柄
//                               0);                // 无分配标志
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "UART驱动安装失败: %s", esp_err_to_name(ret));
//         return ret;
//     }

//     ESP_LOGI(TAG, "串口初始化成功");
//     return ESP_OK;
// }

// /**
//  * @brief 发送原始数据到串口屏
//  * @param data 数据缓冲区
//  * @param len 数据长度
//  * @return int 实际发送的字节数，负数表示错误
//  */
// int tjc_send_raw(const uint8_t *data, size_t len)
// {
//     if (data == NULL || len == 0)
//     {
//         ESP_LOGW(TAG, "发送数据为空");
//         return 0;
//     }

//     // 发送数据 [citation:4]
//     int bytes_written = uart_write_bytes(TJC_UART_PORT_NUM, (const char *)data, len);

//     // 等待数据发送完成 (超时100ms)
//     esp_err_t ret = uart_wait_tx_done(TJC_UART_PORT_NUM, pdMS_TO_TICKS(100));
//     if (ret != ESP_OK)
//     {
//         ESP_LOGW(TAG, "等待发送完成超时");
//     }

//     ESP_LOGD(TAG, "发送 %d 字节", bytes_written);
//     return bytes_written;
// }

// /**
//  * @brief 发送字符串指令到串口屏（自动添加结束符）
//  * @param cmd 指令字符串（不包含结束符）
//  * @return int 实际发送的字节数，负数表示错误
//  *
//  * @note 淘晶驰串口屏指令格式：指令内容 + 0xFF 0xFF 0xFF [citation:5]
//  *       例如：page 0 指令实际发送为 "page 0\xFF\xFF\xFF"
//  */
// int tjc_send_command(const char *cmd)
// {
//     if (cmd == NULL)
//     {
//         return 0;
//     }

//     size_t cmd_len = strlen(cmd);
//     if (cmd_len > TJC_CMD_MAX_LEN - 3)
//     {
//         ESP_LOGE(TAG, "指令过长，最大支持 %d 字节", TJC_CMD_MAX_LEN - 3);
//         return -1;
//     }

//     // 分配缓冲区：指令内容 + 3个结束符
//     uint8_t *tx_buffer = malloc(cmd_len + 3);
//     if (tx_buffer == NULL)
//     {
//         ESP_LOGE(TAG, "内存分配失败");
//         return -2;
//     }

//     // 组装指令
//     memcpy(tx_buffer, cmd, cmd_len);
//     tx_buffer[cmd_len] = TJC_CMD_END1;
//     tx_buffer[cmd_len + 1] = TJC_CMD_END2;
//     tx_buffer[cmd_len + 2] = TJC_CMD_END3;

//     // 发送数据
//     int result = tjc_send_raw(tx_buffer, cmd_len + 3);

//     free(tx_buffer);
//     return result;
// }

// /**
//  * @brief 发送格式化指令到串口屏
//  * @param format 格式化字符串
//  * @param ... 可变参数
//  * @return int 实际发送的字节数，负数表示错误
//  *
//  * @example tjc_printf("page %d", 1);  // 切换到页面1
//  */
// int tjc_printf(const char *format, ...)
// {
//     if (format == NULL)
//     {
//         return 0;
//     }

//     char cmd_buffer[TJC_CMD_MAX_LEN];

//     va_list args;
//     va_start(args, format);
//     int len = vsnprintf(cmd_buffer, sizeof(cmd_buffer), format, args);
//     va_end(args);

//     if (len < 0)
//     {
//         ESP_LOGE(TAG, "格式化指令失败");
//         return -1;
//     }

//     if (len >= TJC_CMD_MAX_LEN)
//     {
//         ESP_LOGW(TAG, "指令可能被截断");
//     }

//     return tjc_send_command(cmd_buffer);
// }

// /**
//  * @brief 清空接收缓冲区
//  */
// void tjc_uart_flush(void)
// {
//     uart_flush(TJC_UART_PORT_NUM);
//     ESP_LOGD(TAG, "清空接收缓冲区");
// }

// /**
//  * @brief 检查串口是否已初始化
//  * @return bool true表示已初始化
//  */
// bool tjc_is_initialized(void)
// {
//     // 简单的检查方式：尝试获取驱动状态
//     size_t free_buf_size;
//     esp_err_t ret = uart_get_tx_buffer_free_size(TJC_UART_PORT_NUM, &free_buf_size);
//     return (ret == ESP_OK);
// }

// /**
//  * @brief 关闭串口并释放资源
//  * @return esp_err_t
//  */
// esp_err_t tjc_uart_deinit(void)
// {
//     ESP_LOGI(TAG, "关闭串口");
//     return uart_driver_delete(TJC_UART_PORT_NUM);
// }