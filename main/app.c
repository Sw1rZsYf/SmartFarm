#include"app.h"
#include "dht.h"
#include "gy30.h"

void read_sensors(float *temperature, float *humidity,
                  int *nh3_voltage, int *h2s_voltage,int *light)
{
    // 读取DHT传感器数据
    dht_read_float_data(DHT_TYPE_DHT11, DHT_GPIO_PIN, humidity, temperature);
    // 读取ADC传感器数据
    adc_system_read(nh3_voltage, h2s_voltage, light);
    // 读取光照传感器数据
    *light = (int)gy30_read_light();
}

