#include "adc.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "ADC_DRIVER";

// 全局变量存储ADC单元句柄
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_oneshot_unit_handle_t adc2_handle = NULL;

// 全局变量存储ADC校准句柄
adc_cali_handle_t cali_handle_GUS = NULL;
adc_cali_handle_t cali_handle_LIGHT = NULL;
// ADC 初始化函数
esp_err_t adc_init(adc_unit_t adc_unit, adc_channel_t channel, adc_atten_t atten)
{
    esp_err_t ret;
    adc_oneshot_unit_handle_t *handle_ptr = NULL;

    // 选择对应的ADC单元句柄指针
    if (adc_unit == ADC_UNIT_1)
    {
        handle_ptr = &adc1_handle;
    }
    else if (adc_unit == ADC_UNIT_2)
    {
        handle_ptr = &adc2_handle;
    }
    else
    {
        ESP_LOGE(TAG, "Invalid ADC unit: %d", adc_unit);
        return ESP_ERR_INVALID_ARG;
    }

    // 如果句柄不存在，创建新的ADC单元
    if (*handle_ptr == NULL)
    {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = adc_unit,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };

        ret = adc_oneshot_new_unit(&init_config, handle_ptr);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create ADC unit %d: %s", adc_unit, esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Created new ADC unit %d", adc_unit);
    }

    // 配置ADC通道
    adc_oneshot_chan_cfg_t config = {
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ret = adc_oneshot_config_channel(*handle_ptr, channel, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "ADC initialized: unit=%d, channel=%d, atten=%d", adc_unit, channel, atten);
    return ESP_OK;
}

// ADC 校准函数
adc_cali_handle_t adc_calibrate(adc_unit_t adc_unit, adc_atten_t atten)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "Using curve fitting calibration");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = adc_unit,
            .chan = ADC_CHANNEL_0, // 校准方案不依赖特定通道
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "Using line fitting calibration");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = adc_unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "ADC calibration successful for unit %d", adc_unit);
    }
    else
    {
        ESP_LOGW(TAG, "ADC calibration not supported or failed for unit %d", adc_unit);
    }

    return handle;
}

// 读取 ADC 电压值
int adc_read_voltage(adc_unit_t adc_unit, adc_channel_t channel, adc_cali_handle_t handle)
{
    int raw = 0;
    int voltage = 0;
    esp_err_t ret;
    adc_oneshot_unit_handle_t adc_handle = NULL;

    // 获取对应的ADC句柄
    if (adc_unit == ADC_UNIT_1)
    {
        adc_handle = adc1_handle;
    }
    else if (adc_unit == ADC_UNIT_2)
    {
        adc_handle = adc2_handle;
    }

    if (adc_handle == NULL)
    {
        ESP_LOGE(TAG, "ADC unit %d not initialized", adc_unit);
        return 0;
    }

    // 多次采样求平均值以提高精度
    int average_count = 32;
    int sum = 0;
    int valid_samples = 0;

    for (int i = 0; i < average_count; i++)
    {
        ret = adc_oneshot_read(adc_handle, channel, &raw);
        if (ret == ESP_OK)
        {
            sum += raw;
            valid_samples++;
        }
        else
        {
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        }
    }

    if (valid_samples == 0)
    {
        ESP_LOGE(TAG, "No valid ADC samples");
        return 0;
    }

    int average_raw = sum / valid_samples;

    // 如果提供了校准句柄，转换为电压
    if (handle != NULL)
    {
        ret = adc_cali_raw_to_voltage(handle, average_raw, &voltage);
        if (ret == ESP_OK)
        {
            ESP_LOGD(TAG, "ADC read: raw=%d, voltage=%dmV", average_raw, voltage);
        }
        else
        {
            ESP_LOGW(TAG, "ADC calibration failed: %s", esp_err_to_name(ret));
            voltage = average_raw; // 返回原始值
        }
    }
    else
    {
        voltage = average_raw;
        ESP_LOGW(TAG, "ADC read without calibration: raw=%d", voltage);
    }

    return voltage;
}

// 获取ADC单元句柄
adc_oneshot_unit_handle_t get_adc_unit_handle(adc_unit_t adc_unit)
{
    if (adc_unit == ADC_UNIT_1)
    {
        return adc1_handle;
    }
    else if (adc_unit == ADC_UNIT_2)
    {
        return adc2_handle;
    }
    return NULL;
}

void adc_system_init(void)
{
    esp_err_t ret;
    // NH3传感器ADC初始化
    ESP_LOGI("MAIN", "Initializing ADC for NH3 sensor...");
    ret = adc_init(ADC_UNIT_GUS, NH3_CHANNEL, ADC_ATTEN);
    if (ret != ESP_OK)
    {
        ESP_LOGE("MAIN", "Failed to initialize NH3 ADC");
        return;
    }
    // H2S传感器ADC初始化
    ESP_LOGI("MAIN", "Initializing ADC for H2S sensor...");
    ret = adc_init(ADC_UNIT_GUS, H2S_CHANNEL, ADC_ATTEN);
    if (ret != ESP_OK)
    {
        ESP_LOGE("MAIN", "Failed to initialize H2S ADC");
        return;
    }
    // Light传感器ADC初始化
    ESP_LOGI("MAIN", "Initializing ADC for Light sensor...");
    ret = adc_init(ADC_UNIT_LIGHT, LIGHT_CHANNEL, ADC_ATTEN);
    if (ret != ESP_OK)
    {
        ESP_LOGE("MAIN", "Failed to initialize Light ADC");
        return;
    }

    // 2. 校准ADC
    ESP_LOGI("MAIN", "Calibrating ADC...");
    cali_handle_LIGHT = adc_calibrate(ADC_UNIT_LIGHT, ADC_ATTEN);
    cali_handle_GUS = adc_calibrate(ADC_UNIT_GUS, ADC_ATTEN);
    if (cali_handle_GUS == NULL)
    {
        ESP_LOGW("MAIN", "ADC calibration not available, using raw values");
    }
}

void adc_system_read(int *nh3_voltage, int *h2s_voltage, int *light_voltage)
{

    *nh3_voltage = adc_read_voltage(ADC_UNIT_GUS, NH3_CHANNEL, cali_handle_GUS);
    *h2s_voltage = adc_read_voltage(ADC_UNIT_GUS, H2S_CHANNEL, cali_handle_GUS);
    *light_voltage = adc_read_voltage(ADC_UNIT_LIGHT, LIGHT_CHANNEL, cali_handle_LIGHT);
}