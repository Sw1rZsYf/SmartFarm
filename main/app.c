#include "app.h"
#include "dht.h"
#include "gy30.h"

void read_sensors(float *temperature, float *humidity,
                  int *nh3_voltage, int *h2s_voltage, int *light)
{
    // 读取DHT传感器数据
    dht_read_float_data(DHT_TYPE_DHT11, DHT_GPIO_PIN, humidity, temperature);
    // 读取ADC传感器数据
    adc_system_read(nh3_voltage, h2s_voltage, light);
    // 读取光照传感器数据
    *light = (int)gy30_read_light();
}

void sim_read_sensors(float *temperature, float *humidity,
                      int *nh3_voltage, int *h2s_voltage, int *light)
{
    // 模拟传感器数据
    *temperature = 25.0 + (rand() % 100) / 10.0; // 25.0 到 35.0 度
    *humidity = 40.0 + (rand() % 600) / 10.0;    // 40.0% 到 100.0%
    *nh3_voltage = 200 + (rand() % 800);         // 200 到 1000 mV
    *h2s_voltage = 150 + (rand() % 850);         // 150 到 1000 mV
    *light = 100 + (rand() % 900);               // 100 到 1000 lx
}