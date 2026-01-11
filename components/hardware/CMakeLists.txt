# 最小化的组件CMakeLists.txt
idf_component_register(
    SRCS 
        "gpio_driver.c" 
        "timer.c" 
        "adc.c" 
        "wifi_config.c"
        "onenet_config.c"
        "gy30.c"
        "sntp_time.c"
        "mqtt_handler.c"
        "mqtt_publisher.c"
        "app.c"
    INCLUDE_DIRS "."             # 头文件目录
    REQUIRES driver "esp_adc" "esp_wifi" "nvs_flash" "esp_netif" "mqtt" "json" "esp_driver_i2c" "bh1750" "lwip"# 依赖的组件
)
# target_link_libraries(${COMPONENT_LIB} INTERFACE cJSON)
