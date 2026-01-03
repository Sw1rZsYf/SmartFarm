#include "gy30.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "GY30";

static i2c_master_bus_handle_t gy30_i2c_handle = NULL;
static bh1750_handle_t bh1750 = NULL;

static void gy30_i2c_init(void) {
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = GY30_I2C_PORT,
        .scl_io_num = GY30_SCL_GPIO_NUM,
        .sda_io_num = GY30_SDA_GPIO_NUM,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,  // 启用内部上拉
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &gy30_i2c_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C总线初始化失败: 0x%x", ret);
    } else {
        ESP_LOGI(TAG, "I2C总线初始化成功");
    }
}

void gy30_init(void) {
    esp_err_t ret;
    
    // 初始化I2C总线
    gy30_i2c_init();
    
    if (gy30_i2c_handle == NULL) {
        ESP_LOGE(TAG, "I2C总线未初始化");
        return;
    }
    
    // 创建BH1750传感器实例
    ret = bh1750_create(gy30_i2c_handle, BH1750_I2C_ADDRESS_DEFAULT, &bh1750);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BH1750创建失败: 0x%x", ret);
        return;
    }
    
    // 上电
    ret = bh1750_power_on(bh1750);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BH1750上电失败: 0x%x", ret);
        return;
    }
    
    // 设置为连续高分辨率模式
    ret = bh1750_set_measure_mode(bh1750, BH1750_CONTINUE_1LX_RES);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置测量模式失败: 0x%x", ret);
        return;
    }
    
    // 等待传感器稳定（BH1750需要120ms的测量时间）
    vTaskDelay(pdMS_TO_TICKS(120));
    
    ESP_LOGI(TAG, "光照传感器初始化成功");
}

float gy30_read_light(void) {
    if (bh1750 == NULL) {
        ESP_LOGE(TAG, "传感器未初始化");
        return -1.0f;
    }
    
    float lux = 0.0f;
    esp_err_t ret = bh1750_get_data(bh1750, &lux);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取光照数据失败: 0x%x", ret);
        return -1.0f;
    }
    
    // BH1750在连续测量模式下，建议读取间隔至少120ms
    vTaskDelay(pdMS_TO_TICKS(120));
    
    return lux;
}

void gy30_deinit(void) {
    if (bh1750 != NULL) {
        // 先进入低功耗模式
        bh1750_power_down(bh1750);
        // 删除传感器实例
        bh1750_delete(bh1750);
        bh1750 = NULL;
        ESP_LOGI(TAG, "BH1750已删除");
    }
    
    if (gy30_i2c_handle != NULL) {
        // 删除I2C总线
        i2c_del_master_bus(gy30_i2c_handle);
        gy30_i2c_handle = NULL;
        ESP_LOGI(TAG, "I2C总线已删除");
    }
}

// 可选：设置测量时间函数
esp_err_t gy30_set_measure_time(uint8_t measure_time) {
    if (bh1750 == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return bh1750_set_measure_time(bh1750, measure_time);
}

// // 可选：获取原始数据的函数（调试用）
// esp_err_t gy30_read_raw_data(uint16_t *raw_data) {
//     if (bh1750 == NULL) {
//         return ESP_ERR_INVALID_STATE;
//     }
    
//     uint8_t buffer[2];
//     bh1750_dev_t *sens = (bh1750_dev_t *)bh1750;
//     esp_err_t ret = i2c_master_receive(sens->i2c_handle, buffer, sizeof(buffer), pdMS_TO_TICKS(1000));
    
//     if (ret == ESP_OK) {
//         *raw_data = (buffer[0] << 8) | buffer[1];
//     }
    
//     return ret;
// }