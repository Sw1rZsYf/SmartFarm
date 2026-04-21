#include "farm_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    farm_app_start();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
