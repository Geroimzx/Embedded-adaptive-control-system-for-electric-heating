#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "model/system_state.h"
#include "model/settings_manager.h"
#include "model/main_control.h"
#include "controller/temp_setpoint_manager.h"
#include "controller/schedule_manager.h"

static const char *TAG = "WEB_SERVER";

// Оголошення вбудованих файлів
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t style_css_start[]  asm("_binary_style_css_start");
extern const uint8_t style_css_end[]    asm("_binary_style_css_end");
extern const uint8_t app_js_start[]     asm("_binary_app_js_start");
extern const uint8_t app_js_end[]       asm("_binary_app_js_end");

extern const char* state_to_string(system_state_t state);

static size_t get_embedded_file_len(const uint8_t *start, const uint8_t *end) {
    size_t len = end - start;
    while (len > 0 && start[len - 1] == 0) {
        len--;
    }
    return len;
}

// --- STATIC FILE HANDLERS ---
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    size_t len = get_embedded_file_len(index_html_start, index_html_end);
    httpd_resp_send(req, (const char *)index_html_start, len);
    return ESP_OK;
}

static esp_err_t style_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    size_t len = get_embedded_file_len(style_css_start, style_css_end);
    httpd_resp_send(req, (const char *)style_css_start, len);
    return ESP_OK;
}

static esp_err_t app_js_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    size_t len = get_embedded_file_len(app_js_start, app_js_end);
    httpd_resp_send(req, (const char *)app_js_start, len);
    return ESP_OK;
}

// --- API STATUS ---
static esp_err_t api_status_get_handler(httpd_req_t *req) {
    sensors_state_t state;
    system_state_get(&state);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "room_temp", state.temperature_c_sensor1);
    cJSON_AddNumberToObject(root, "rad_temp", state.temperature_c_sensor2);
    cJSON_AddNumberToObject(root, "outside_temp", state.temperature_c_outside);
    cJSON_AddNumberToObject(root, "current_setpoint", state.current_setpoint);
    cJSON_AddBoolToObject(root, "relay", state.relay_is_on);
    cJSON_AddStringToObject(root, "state", state_to_string(state.system_state));
    cJSON_AddNumberToObject(root, "manual_setpoint", temp_setpoint_manager_get());

    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// --- API ACTION (Set Mode & Set Temp) ---
static esp_err_t api_action_post_handler(httpd_req_t *req) {
    char buf[200];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    cJSON *action = cJSON_GetObjectItem(root, "action");
    
    if (action && strcmp(action->valuestring, "set_mode") == 0) {
        cJSON *mode_item = cJSON_GetObjectItem(root, "mode");
        if(mode_item) {
            const char *m = mode_item->valuestring;
            system_state_t new_state = STATE_OFF;
            if (strcmp(m, "MANUAL") == 0) new_state = STATE_MANUAL;
            else if (strcmp(m, "PROGRAMMED") == 0) new_state = STATE_PROGRAMMED;
            else if (strcmp(m, "OFF") == 0) new_state = STATE_OFF;
            else if (strcmp(m, "ADAPTIVE") == 0) new_state = STATE_ADAPTIVE;
            main_control_change_state(new_state);
        }
    }
    else if (action && strcmp(action->valuestring, "set_temp") == 0) {
        cJSON *val = cJSON_GetObjectItem(root, "value");
        if (val) {
            float t = (float)val->valuedouble;
            temp_setpoint_manager_set(t);
        }
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// --- API SCHEDULE GET ---
static esp_err_t api_schedule_get_handler(httpd_req_t *req) {
    extern void schedule_manager_get_full(week_schedule_t *out);
    
    week_schedule_t sched;
    schedule_manager_get_full(&sched);

    cJSON *root = cJSON_CreateObject();
    cJSON *days_arr = cJSON_CreateArray();
    
    for(int i=0; i<7; i++) {
        cJSON *day_obj = cJSON_CreateObject();
        cJSON *points_arr = cJSON_CreateArray();
        
        for(int p=0; p < sched.days[i].num_points; p++) {
            cJSON *pt = cJSON_CreateObject();
            cJSON_AddNumberToObject(pt, "h", sched.days[i].points[p].hour);
            cJSON_AddNumberToObject(pt, "m", sched.days[i].points[p].minute);
            cJSON_AddNumberToObject(pt, "t", sched.days[i].points[p].temperature);
            cJSON_AddItemToArray(points_arr, pt);
        }
        
        cJSON_AddItemToObject(day_obj, "points", points_arr);
        cJSON_AddItemToArray(days_arr, day_obj);
    }
    
    cJSON_AddItemToObject(root, "days", days_arr);

    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// --- API SCHEDULE POST ---
static esp_err_t api_schedule_post_handler(httpd_req_t *req) {
    char *buf = malloc(req->content_len + 1);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }
    
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) { free(buf); return ESP_FAIL; }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if(!root) { free(buf); return ESP_FAIL; }

    week_schedule_t new_sched;
    memset(&new_sched, 0, sizeof(week_schedule_t));
    
    cJSON *days_arr = cJSON_GetObjectItem(root, "days");
    if (days_arr && cJSON_IsArray(days_arr)) {
        for(int i=0; i<7; i++) {
            cJSON *day_obj = cJSON_GetArrayItem(days_arr, i);
            if(!day_obj) continue;
            
            cJSON *pts = cJSON_GetObjectItem(day_obj, "points");
            if(pts && cJSON_IsArray(pts)) {
                int count = cJSON_GetArraySize(pts);
                if(count > MAX_SCHEDULE_POINTS_PER_DAY) count = MAX_SCHEDULE_POINTS_PER_DAY;
                
                new_sched.days[i].num_points = count;
                for(int p=0; p<count; p++) {
                    cJSON *pt = cJSON_GetArrayItem(pts, p);
                    new_sched.days[i].points[p].hour = cJSON_GetObjectItem(pt, "h")->valueint;
                    new_sched.days[i].points[p].minute = cJSON_GetObjectItem(pt, "m")->valueint;
                    new_sched.days[i].points[p].temperature = (float)cJSON_GetObjectItem(pt, "t")->valuedouble;
                }
            }
        }
    }
    
    schedule_manager_save_schedule(&new_sched);
    
    cJSON_Delete(root);
    free(buf);
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// --- API SETTINGS GET ---
static esp_err_t api_settings_get_handler(httpd_req_t *req) {
    const app_settings_t *cfg = settings_get();
    cJSON *root = cJSON_CreateObject();
    
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi, "ssid", cfg->wifi.sta_ssid);
    cJSON_AddStringToObject(wifi, "pass", ""); 
    cJSON_AddItemToObject(root, "wifi", wifi);

    cJSON *mqtt = cJSON_CreateObject();
    cJSON_AddStringToObject(mqtt, "host", cfg->mqtt.host);
    cJSON_AddNumberToObject(mqtt, "port", cfg->mqtt.port);
    cJSON_AddStringToObject(mqtt, "token", cfg->mqtt.token);
    cJSON_AddItemToObject(root, "mqtt", mqtt);

    cJSON *geo = cJSON_CreateObject();
    cJSON_AddNumberToObject(geo, "lat", cfg->geo.lat);
    cJSON_AddNumberToObject(geo, "lon", cfg->geo.lon);
    cJSON_AddNumberToObject(geo, "interval", cfg->geo.interval_min);
    cJSON_AddItemToObject(root, "geo", geo);

    cJSON *control = cJSON_CreateObject();
    cJSON_AddNumberToObject(control, "pwm_cycle_s", cfg->control.pwm_cycle_s);
    cJSON *pid = cJSON_CreateObject();
    cJSON_AddNumberToObject(pid, "kp", cfg->control.pid.kp);
    cJSON_AddNumberToObject(pid, "ki", cfg->control.pid.ki);
    cJSON_AddNumberToObject(pid, "kd", cfg->control.pid.kd);
    cJSON_AddItemToObject(control, "pid", pid);

    cJSON *limits = cJSON_CreateObject();
    cJSON_AddNumberToObject(limits, "rad_max", cfg->control.limits.rad_max);
    cJSON_AddNumberToObject(limits, "room_min", cfg->control.limits.room_min);
    cJSON_AddNumberToObject(limits, "room_max", cfg->control.limits.room_max);
    cJSON_AddItemToObject(control, "limits", limits);
    
    cJSON_AddItemToObject(root, "control", control);

    const char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    free((void *)json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// --- API SETTINGS POST ---
static esp_err_t api_settings_post_handler(httpd_req_t *req) {
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    app_settings_t *cfg = settings_get_writeable();

    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if(wifi) {
        cJSON *s = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *p = cJSON_GetObjectItem(wifi, "pass");
        if(s) strncpy(cfg->wifi.sta_ssid, s->valuestring, sizeof(cfg->wifi.sta_ssid)-1);
        if(p && strlen(p->valuestring) > 0) strncpy(cfg->wifi.sta_pass, p->valuestring, sizeof(cfg->wifi.sta_pass)-1);
    }
    
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if(mqtt) {
        cJSON *h = cJSON_GetObjectItem(mqtt, "host");
        cJSON *p = cJSON_GetObjectItem(mqtt, "port");
        cJSON *t = cJSON_GetObjectItem(mqtt, "token");
        if(h) strncpy(cfg->mqtt.host, h->valuestring, sizeof(cfg->mqtt.host)-1);
        if(p) cfg->mqtt.port = p->valueint;
        if(t) strncpy(cfg->mqtt.token, t->valuestring, sizeof(cfg->mqtt.token)-1);
    }

    cJSON *geo = cJSON_GetObjectItem(root, "geo");
    if(geo) {
        cJSON *lat = cJSON_GetObjectItem(geo, "lat");
        cJSON *lon = cJSON_GetObjectItem(geo, "lon");
        cJSON *inv = cJSON_GetObjectItem(geo, "interval");
        
        if(lat) cfg->geo.lat = (float)lat->valuedouble;
        if(lon) cfg->geo.lon = (float)lon->valuedouble;
        if(inv) cfg->geo.interval_min = inv->valueint;
    }

    cJSON *ctrl = cJSON_GetObjectItem(root, "control");
    if(ctrl) {
        cJSON *pwm = cJSON_GetObjectItem(ctrl, "pwm_cycle_s");
        if (pwm) {
            cfg->control.pwm_cycle_s = pwm->valueint;
        }
        cJSON *pid = cJSON_GetObjectItem(ctrl, "pid");
        if(pid) {
             cfg->control.pid.kp = cJSON_GetObjectItem(pid, "kp")->valuedouble;
             cfg->control.pid.ki = cJSON_GetObjectItem(pid, "ki")->valuedouble;
             cfg->control.pid.kd = cJSON_GetObjectItem(pid, "kd")->valuedouble;
        }
        cJSON *lim = cJSON_GetObjectItem(ctrl, "limits");
        if(lim) {
            cfg->control.limits.rad_max = cJSON_GetObjectItem(lim, "rad_max")->valuedouble;
            cfg->control.limits.room_min = cJSON_GetObjectItem(lim, "room_min")->valuedouble;
            cfg->control.limits.room_max = cJSON_GetObjectItem(lim, "room_max")->valuedouble;
        }
    }

    cJSON_Delete(root);
    settings_save();
    httpd_resp_sendstr(req, "OK");
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

// --- START SERVER ---
esp_err_t start_web_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 15;
    config.stack_size = 10240;

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/", .method=HTTP_GET, .handler=root_get_handler});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/style.css", .method=HTTP_GET, .handler=style_get_handler});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/app.js", .method=HTTP_GET, .handler=app_js_get_handler});
        
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/api/status", .method=HTTP_GET, .handler=api_status_get_handler});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/api/action", .method=HTTP_POST, .handler=api_action_post_handler});
        
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/api/settings", .method=HTTP_GET, .handler=api_settings_get_handler});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/api/settings", .method=HTTP_POST, .handler=api_settings_post_handler});

        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/api/schedule", .method=HTTP_GET, .handler=api_schedule_get_handler});
        httpd_register_uri_handler(server, &(httpd_uri_t){.uri="/api/schedule", .method=HTTP_POST, .handler=api_schedule_post_handler});
        
        ESP_LOGI(TAG, "Web Server started!");
        return ESP_OK;
    }
    return ESP_FAIL;
}