#include "mqtt_task.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "model/system_state.h"

static void mqtt_publish_task(void *arg)
{
    sensors_state_t data;

    while (1) {
        system_state_get(&data);
        mqtt_publish_state(&data);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void mqtt_publish_task_start(void)
{
    xTaskCreate(mqtt_publish_task, "mqtt_publish_task", 4096, NULL, 5, NULL);
}