// /**
//  * @file tjc_driver.h
//  * @brief 淘晶驰T1系列串口屏驱动头文件
//  */

// #ifndef TJC_DRIVER_H
// #define TJC_DRIVER_H

// #include <stdint.h>
// #include <stdbool.h>
// #include <stdarg.h>
// #include "esp_err.h"

// #ifdef __cplusplus
// extern "C"
// {
// #endif

//     /**
//      * @brief 初始化串口
//      */
//     esp_err_t tjc_uart_init(void);

//     /**
//      * @brief 发送原始数据到串口屏
//      */
//     int tjc_send_raw(const uint8_t *data, size_t len);

//     /**
//      * @brief 发送字符串指令到串口屏（自动添加结束符）
//      */
//     int tjc_send_command(const char *cmd);

//     /**
//      * @brief 发送格式化指令到串口屏
//      */
//     int tjc_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

//     /**
//      * @brief 清空接收缓冲区
//      */
//     void tjc_uart_flush(void);

//     /**
//      * @brief 检查串口是否已初始化
//      */
//     bool tjc_is_initialized(void);

//     /**
//      * @brief 关闭串口并释放资源
//      */
//     esp_err_t tjc_uart_deinit(void);

// #ifdef __cplusplus
// }
// #endif

// #endif /* TJC_DRIVER_H */