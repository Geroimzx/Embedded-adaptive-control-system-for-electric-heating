#include "esp_log.h"
#include "cJSON.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <mqtt_client.h> 
#include <esp_event.h>

#include "networking/mqtt_client.h"
#include "model/main_control.h"
#include "model/settings_manager.h"

static const char *TAG = "TB_MQTT";

static esp_mqtt_client_handle_t client = NULL;

#define MQTT_TOPIC     "v1/devices/me/telemetry"

#define STR(x) #x
#define STRINGIFY(x) STR(x)

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to ThingsBoard!");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected from ThingsBoard");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGE(TAG, "Last error code reported from tls stack: 0x%x", event->error_handle->esp_tls_stack_err);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_init(void)
{
    const app_settings_t *cfg = settings_get();

    char uri_buf[128];
    int port = (cfg->mqtt.port > 0) ? cfg->mqtt.port : 1883;
    
    const char* host = (strlen(cfg->mqtt.host) > 0) ? cfg->mqtt.host : "demo.thingsboard.io";

    snprintf(uri_buf, sizeof(uri_buf), "mqtt://%s:%d", host, port);
    
    ESP_LOGI(TAG, "Initializing MQTT to: %s with Token: %s", uri_buf, cfg->mqtt.token);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri_buf,
        .credentials.username = cfg->mqtt.token,
        .session.keepalive = 60,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "ThingsBoard MQTT initialized");
}

void mqtt_publish_state(const sensors_state_t *st)
{
    if (!client) {
        ESP_LOGE(TAG, "MQTT client not initialized!");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to create cJSON object");
        return;
    }

    cJSON_AddNumberToObject(root, "temperature_room", st->temperature_c_sensor1);
    cJSON_AddNumberToObject(root, "temperature_radiator", st->temperature_c_sensor2);
    cJSON_AddNumberToObject(root, "temperature_outside", st->temperature_c_outside);
    cJSON_AddNumberToObject(root, "current_setpiont", st->current_setpoint);
    cJSON_AddBoolToObject(root, "relay_is_on", st->relay_is_on);
    cJSON_AddBoolToObject(root, "presence_state", st->presence_state);

    const char *state_str = state_to_string(st->system_state);
    cJSON_AddStringToObject(root, "system_state", state_str);

    char *json_data = cJSON_PrintUnformatted(root);
    if (json_data == NULL) {
        ESP_LOGE(TAG, "Failed to print cJSON to string");
        cJSON_Delete(root);
        return;
    }

    int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC, json_data, 0, 1, 0);

    if (msg_id >= 0) {
        ESP_LOGI(TAG, "Data sent to TB (msg_id=%d): %s", msg_id, json_data);
    } else {
        ESP_LOGE(TAG, "MQTT publish failed!");
    }

    cJSON_Delete(root);
    free(json_data);
}