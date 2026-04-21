/*
 * farm_display.h - 农场显示层接口
 */
#ifndef FARM_DISPLAY_H
#define FARM_DISPLAY_H

#include "app.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void farm_display_init(void);
    void farm_display_show_boot(const char *line1, const char *line2);
    void farm_display_show_status(const char *wifi_state, const char *cloud_state, const char *device_state);
    void farm_display_show_sensor(const sensor_data_t *sensor_data);
    void farm_display_show_time(const char *time_str);

#ifdef __cplusplus
}
#endif

#endif /* FARM_DISPLAY_H */