#pragma once
#include "bh1750.h"
#include "driver/i2c_master.h"

// 引脚定义
#define GY30_SDA_GPIO_NUM 5 // 根据实际连接修改
#define GY30_SCL_GPIO_NUM 6 // 根据实际连接修改
#define GY30_I2C_PORT I2C_NUM_0
#define BH1750_I2C_ADDRESS_DEFAULT 0x23

// 初始化光照传感器
void gy30_init(void);

// 读取光照强度（单位：Lux）
float gy30_read_light(void);

// 去初始化，释放资源
void gy30_deinit(void);