/**
 * @file tjc_driver.h
 * @brief 淘晶驰T1系列串口屏驱动头文件
 */

#ifndef TJC_DRIVER_H
#define TJC_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief 初始化串口
     */
    esp_err_t tjc_uart_init(void);

    /**
     * @brief 发送原始数据到串口屏
     */
    int tjc_send_raw(const uint8_t *data, size_t len);

    /**
     * @brief 发送字符串指令到串口屏（自动添加结束符）
     */
    int tjc_send_command(const char *cmd);

    /**
     * @brief 发送格式化指令到串口屏
     */
    int tjc_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

    /**
     * @brief 清空接收缓冲区
     */
    void tjc_uart_flush(void);

    /**
     * @brief 检查串口是否已初始化
     */
    bool tjc_is_initialized(void);

    /**
     * @brief 关闭串口并释放资源
     */
    esp_err_t tjc_uart_deinit(void);

    esp_err_t tjc_sent_sensor(float *temperature, float *humidity,
                              int *nh3_voltage, int *h2s_voltage, int *light);
    /**
     * @brief 读取一条完整的串口屏指令（以三个0xFF结尾）
     * @param buffer 存放指令的缓冲区（不含结束符）
     * @param buf_size 缓冲区大小
     * @param timeout 等待超时时间（ms）
     * @return int 实际接收到的指令长度（不含结束符），0表示无数据，负数表示错误
     */
    int tjc_receive_command(char *buffer, size_t buf_size, uint32_t timeout_ms);
#ifdef __cplusplus
}
#endif

#endif /* TJC_DRIVER_H */