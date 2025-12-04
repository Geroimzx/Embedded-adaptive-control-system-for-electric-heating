#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h" 
#include "cJSON.h"

static const char *TAG = "WEATHER";

#define WEATHER_HTTP_BUFFER_SIZE 2048
#define WEATHER_DEFAULT_TEMP -1000.0f

// Структура для стану
typedef struct {
    float lat;
    float lon;
    int interval_min;
    float current_temp;
    time_t last_update;
    TaskHandle_t task_handle;
    SemaphoreHandle_t mutex;
    bool is_running;
} weather_ctx_t;

// Структура для збору відповіді
typedef struct {
    char *buffer;
    int pos;
    int max_len;
} response_buffer_t;

static weather_ctx_t s_ctx = {
    .current_temp = WEATHER_DEFAULT_TEMP,
    .mutex = NULL,
    .is_running = false
};

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->user_data) {
                response_buffer_t *resp = (response_buffer_t *)evt->user_data;
                
                if (resp->pos + evt->data_len < resp->max_len) {
                    memcpy(resp->buffer + resp->pos, evt->data, evt->data_len);
                    resp->pos += evt->data_len;
                    resp->buffer[resp->pos] = 0;
                } else {
                    ESP_LOGW(TAG, "Buffer overflow! Received data too long.");
                }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void _perform_weather_request(void) {
    char url[256];
    char *rx_buffer = malloc(WEATHER_HTTP_BUFFER_SIZE);
    if (!rx_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTTP buffer");
        return;
    }

    response_buffer_t resp_data = {
        .buffer = rx_buffer,
        .pos = 0,
        .max_len = WEATHER_HTTP_BUFFER_SIZE
    };

    float lat, lon;
    xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
    lat = s_ctx.lat;
    lon = s_ctx.lon;
    xSemaphoreGive(s_ctx.mutex);

    snprintf(url, sizeof(url), 
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m", 
             lat, lon);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .user_data = &resp_data,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size_tx = 512,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        free(rx_buffer);
        return;
    }

    ESP_LOGI(TAG, "Requesting: %s", url);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_len = esp_http_client_get_content_length(client);
        
        ESP_LOGI(TAG, "HTTP Status: %d, Content Len: %d", status_code, content_len);

        if (status_code == 200) {
            ESP_LOGI(TAG, "Raw Response: %s", rx_buffer);

            cJSON *json = cJSON_Parse(rx_buffer);
            if (json) {
                cJSON *current = cJSON_GetObjectItem(json, "current");
                if (current) {
                    cJSON *temp_item = cJSON_GetObjectItem(current, "temperature_2m");
                    if (cJSON_IsNumber(temp_item)) {
                        
                        xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
                        s_ctx.current_temp = (float)temp_item->valuedouble;
                        time(&s_ctx.last_update);
                        xSemaphoreGive(s_ctx.mutex);
                        
                        ESP_LOGI(TAG, "Parsed Temp: %.2f", s_ctx.current_temp);
                    } else {
                        ESP_LOGW(TAG, "JSON: 'temperature_2m' not found or not a number");
                    }
                } else {
                    ESP_LOGW(TAG, "JSON: 'current' object not found");
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "JSON Parse Error! Check if response is valid JSON.");
            }
        } else {
            ESP_LOGW(TAG, "Server returned error status: %d", status_code);
            ESP_LOGW(TAG, "Response body: %s", rx_buffer);
        }
    } else {
        ESP_LOGE(TAG, "HTTP Request Failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(rx_buffer);
}

static void weather_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (s_ctx.is_running) {
        _perform_weather_request();

        int delay_min;
        xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
        delay_min = s_ctx.interval_min;
        xSemaphoreGive(s_ctx.mutex);

        if (delay_min < 1) delay_min = 1;

        for (int i = 0; i < delay_min * 60; i++) {
            if (!s_ctx.is_running) break;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    vTaskDelete(NULL);
}

bool weather_init(float latitude, float longitude, int update_interval_min) {
    if (s_ctx.is_running) return true;

    if (s_ctx.mutex == NULL) {
        s_ctx.mutex = xSemaphoreCreateMutex();
    }

    s_ctx.lat = latitude;
    s_ctx.lon = longitude;
    s_ctx.interval_min = update_interval_min;
    s_ctx.current_temp = WEATHER_DEFAULT_TEMP;
    s_ctx.is_running = true;

    BaseType_t res = xTaskCreate(weather_task, "weather_task", 8192, NULL, 5, &s_ctx.task_handle);
    return (res == pdPASS);
}

void weather_deinit(void) {
    s_ctx.is_running = false;
}

void weather_set_location(float latitude, float longitude) {
    if (s_ctx.mutex) {
        xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
        s_ctx.lat = latitude;
        s_ctx.lon = longitude;
        s_ctx.current_temp = WEATHER_DEFAULT_TEMP; 
        xSemaphoreGive(s_ctx.mutex);
    }
}

void weather_set_update_interval(int minutes) {
    if (s_ctx.mutex) {
        xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
        s_ctx.interval_min = minutes;
        xSemaphoreGive(s_ctx.mutex);
    }
}

float weather_get_temperature(void) {
    float temp = WEATHER_DEFAULT_TEMP;
    if (s_ctx.mutex) {
        xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
        temp = s_ctx.current_temp;
        xSemaphoreGive(s_ctx.mutex);
    }
    return temp;
}

time_t weather_get_last_update_time(void) {
    time_t time = 0;
    if (s_ctx.mutex) {
        xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
        time = s_ctx.last_update;
        xSemaphoreGive(s_ctx.mutex);
    }
    return time;
}