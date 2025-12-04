#include "view/display_manager.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char* TAG = "DisplayView";
static u8g2_t u8g2;

static const unsigned char image_init_icon[] = {0xff,0xff,0x3f,0xff,0xff,0x3f,0x0c,0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00,0x0c,0xcc,0xc0,0x0c,0xcc,0xc0,0x0c,0xcc,0xff,0x0c,0xcc,0xff,0x0c,0x30,0x3f,0x03,0x30,0x3f,0x03,0xc0,0xcc,0x00,0xc0,0xcc,0x00,0x00,0x33,0x00,0x00,0x33,0x00,0x00,0x33,0x00,0x00,0x33,0x00,0xc0,0xc0,0x00,0xc0,0xc0,0x00,0x30,0x0c,0x03,0x30,0x0c,0x03,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3f,0x0c,0x0c,0x3f,0x0c,0xcc,0xff,0x0c,0xcc,0xff,0x0c,0xfc,0xff,0x0f,0xfc,0xff,0x0f,0xff,0xff,0x3f,0xff,0xff,0x3f};

static const unsigned char image_time_icon[] = {0xe0,0x03,0x18,0x0c,0x94,0x14,0x82,0x20,0x86,0x30,0x81,0x40,0x81,0x40,0x87,0x70,0x01,0x41,0x01,0x42,0x06,0x34,0x02,0x20,0x94,0x14,0x98,0x0c,0xe0,0x03,0x00,0x00};
static const unsigned char image_wifi_connected_bits[] = {0x80,0x0f,0x00,0xe0,0x3f,0x00,0x78,0xf0,0x00,0x9c,0xcf,0x01,0xee,0xbf,0x03,0xf7,0x78,0x07,0x3a,0xe7,0x02,0xdc,0xdf,0x01,0xe8,0xb8,0x00,0x70,0x77,0x00,0xa0,0x2f,0x00,0xc0,0x1d,0x00,0x80,0x0a,0x00,0x00,0x07,0x00,0x00,0x02,0x00,0x00,0x00,0x00};
static const unsigned char image_wifi_not_connected_bits[] = {0x84,0x0f,0x00,0x68,0x30,0x00,0x10,0xc0,0x00,0xa4,0x0f,0x01,0x42,0x30,0x02,0x91,0x40,0x04,0x08,0x85,0x00,0xc4,0x1a,0x01,0x20,0x24,0x00,0x10,0x4a,0x00,0x80,0x15,0x00,0x40,0x20,0x00,0x00,0x42,0x00,0x00,0x85,0x00,0x00,0x02,0x01,0x00,0x00,0x00};
static const unsigned char image_temperature_icon[] = {0x38,0x00,0x44,0x40,0xd4,0xa0,0x54,0x40,0xd4,0x1c,0x54,0x06,0xd4,0x02,0x54,0x02,0x54,0x06,0x92,0x1c,0x39,0x01,0x75,0x01,0x7d,0x01,0x39,0x01,0x82,0x00,0x7c,0x00};
static const unsigned char arrow_up_bits[] = {0x18, 0x3C, 0x7E, 0xDB, 0x18, 0x18, 0x18, 0x00};
static const unsigned char arrow_down_bits[] = {0x18, 0x18, 0x18, 0xDB, 0x7E, 0x3C, 0x18, 0x00};

static const unsigned char image_alert_icon[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x03, 0x00, 0xc0, 
  0x07, 0x00, 0xe0, 0x0f, 0x00, 0xf0, 0x1f, 0x00, 0xf8, 0x3f, 0x00, 0xdc, 
  0x7f, 0x00, 0xce, 0xff, 0x00, 0xc7, 0xff, 0x01, 0xc3, 0xff, 0x03, 0xe1, 
  0xff, 0x07, 0xf0, 0xff, 0x0f, 0xf8, 0xcf, 0x1f, 0x7c, 0x00, 0x3e, 0x3e, 
  0x00, 0x7c, 0x1e, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void display_manager_init(gpio_num_t PIN_SDA, gpio_num_t PIN_SCL) {
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = PIN_SDA;
    hal.bus.i2c.scl = PIN_SCL;
    u8g2_esp32_hal_init(hal);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2, U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb
    );
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    ESP_LOGI(TAG, "Display initialized");
}

void render_splash_screen(void) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawXBM(&u8g2, 53, 16, 22, 32, image_init_icon);
    u8g2_SendBuffer(&u8g2);
}

void render_main_screen(const main_screen_data_t* data) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFontMode(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);
    
    u8g2_DrawXBM(&u8g2, 110, 4, 16, 16, image_temperature_icon);
    u8g2_DrawXBM(&u8g2, 110, 26, 15, 16, image_time_icon);
    
    if(data->wifi_connected) {
        u8g2_DrawXBM(&u8g2, 2, 2, 19, 16, image_wifi_connected_bits);
    } else {
        u8g2_DrawXBM(&u8g2, 2, 2, 19, 16, image_wifi_not_connected_bits);
    }

    u8g2_SetFont(&u8g2, u8g2_font_profont12_tr);
    u8g2_DrawStr(&u8g2, 2, 28, data->mode_str);

    char temp_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.1f", data->temperature);
    u8g2_SetFont(&u8g2, u8g2_font_profont29_tr);
    u8g2_DrawStr(&u8g2, 44, 22, temp_str);
    
    u8g2_SetFont(&u8g2, u8g2_font_profont17_tr);
    u8g2_DrawStr(&u8g2, 54, 39, data->time_str);

    u8g2_DrawStr(&u8g2, 31, 60, data->date_str);

    u8g2_SendBuffer(&u8g2);
}

void render_mode_select_screen(system_state_t preview_state)
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFontMode(&u8g2, 1);

    system_state_t selectable_states[] = {
        STATE_MANUAL,
        STATE_ADAPTIVE,
        STATE_PROGRAMMED,
        STATE_ANTI_FREEZE
    };

    const char *state_names[] = {
        "MANUAL",
        "ADAPTIVE",
        "PROGRAMMED",
        "ANTI-FREEZE"
    };

    int count = sizeof(selectable_states) / sizeof(selectable_states[0]);
    int current_idx = 0;

    for (int i = 0; i < count; i++) {
        if (preview_state == selectable_states[i]) {
            current_idx = i;
            break;
        }
    }

    int idx_up   = (current_idx + 1) % count;
    int idx_down = (current_idx - 1 + count) % count;

    u8g2_DrawXBM(&u8g2, 60, 4, 8, 8, arrow_up_bits);

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tr);
    int w_up = u8g2_GetStrWidth(&u8g2, state_names[idx_up]);
    u8g2_DrawStr(&u8g2, (128 - w_up) / 2, 22, state_names[idx_up]);

    u8g2_SetFont(&u8g2, u8g2_font_profont22_tr);
    int w_curr = u8g2_GetStrWidth(&u8g2, state_names[current_idx]);
    u8g2_DrawStr(&u8g2, (128 - w_curr) / 2, 42, state_names[current_idx]);

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tr);
    int w_down = u8g2_GetStrWidth(&u8g2, state_names[idx_down]);
    u8g2_DrawStr(&u8g2, (128 - w_down) / 2, 52, state_names[idx_down]);

    u8g2_DrawXBM(&u8g2, 60, 56, 8, 8, arrow_down_bits);

    u8g2_SendBuffer(&u8g2);
}

void render_temperature_select_screen(float current_temp)
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFontMode(&u8g2, 1);

    const float STEP = 0.25f;
    float temp_up   = current_temp + STEP;
    float temp_down = current_temp - STEP;

    char buf_up[16], buf_curr[16], buf_down[16];

    snprintf(buf_up, sizeof(buf_up), "%.2f", temp_up);
    snprintf(buf_curr, sizeof(buf_curr), "%.2f", current_temp);
    snprintf(buf_down, sizeof(buf_down), "%.2f", temp_down);

    u8g2_DrawXBM(&u8g2, 60, 4, 8, 8, arrow_up_bits);

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tr);
    int w_up = u8g2_GetStrWidth(&u8g2, buf_up);
    u8g2_DrawStr(&u8g2, (128 - w_up) / 2, 22, buf_up);

    u8g2_SetFont(&u8g2, u8g2_font_profont22_tr);
    int w_curr = u8g2_GetStrWidth(&u8g2, buf_curr);
    u8g2_DrawStr(&u8g2, (128 - w_curr) / 2, 42, buf_curr);

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tr);
    int w_down = u8g2_GetStrWidth(&u8g2, buf_down);
    u8g2_DrawStr(&u8g2, (128 - w_down) / 2, 52, buf_down);

    u8g2_DrawXBM(&u8g2, 60, 56, 8, 8, arrow_down_bits);

    u8g2_SendBuffer(&u8g2);
}

void render_info_screen(const char* ap_ssid, const char* ap_pass, const char* ip_addr) {
    u8g2_ClearBuffer(&u8g2);
    
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tr);
    u8g2_DrawStr(&u8g2, 0, 9, "AP Config Mode");
    u8g2_DrawHLine(&u8g2, 0, 11, 128);

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tr);
    u8g2_DrawStr(&u8g2, 0, 22, "SSID:");
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tr);
    u8g2_DrawStr(&u8g2, 0, 33, ap_ssid ? ap_ssid : "N/A");

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tr);
    u8g2_DrawStr(&u8g2, 0, 44, "PASS:");
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tr);
    u8g2_DrawStr(&u8g2, 35, 44, (ap_pass && strlen(ap_pass)>0) ? ap_pass : "Open");

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tr);
    u8g2_DrawStr(&u8g2, 0, 60, "IP:");
    u8g2_DrawStr(&u8g2, 20, 60, ip_addr ? ip_addr : "...");
    
    u8g2_SendBuffer(&u8g2);
}

void render_emergency_screen(int error_code) {
    u8g2_ClearBuffer(&u8g2);

    u8g2_DrawXBM(&u8g2, 4, 20, 24, 24, image_alert_icon);

    u8g2_SetFont(&u8g2, u8g2_font_profont12_tr);
    u8g2_DrawStr(&u8g2, 36, 10, "! EMERGENCY !");
    u8g2_DrawHLine(&u8g2, 36, 12, 90);

    const char *err_msg = "Unknown Error";
    switch (error_code) {
        case 1:
            err_msg = "Room Sens Fail";
            break;
        case 2:
            err_msg = "Rad Sens Fail";
            break;
        case 3:
            err_msg = "Temp Spike";
            break;
        case 4:
            err_msg = "Rad Overheat!";
            break;
        case 5:
            err_msg = "No Weather Data";
            break;
        default:
            err_msg = "System Fail";
            break;
    }

    char code_buf[16];
    snprintf(code_buf, sizeof(code_buf), "Code: %d", error_code);
    
    u8g2_SetFont(&u8g2, u8g2_font_profont17_tr);
    u8g2_DrawStr(&u8g2, 36, 32, code_buf);

    u8g2_SetFont(&u8g2, u8g2_font_profont12_tr);
    u8g2_DrawStr(&u8g2, 36, 48, err_msg);

    u8g2_SetFont(&u8g2, u8g2_font_profont10_tr);
    u8g2_DrawStr(&u8g2, 36, 62, "Check & Reset");

    u8g2_SendBuffer(&u8g2);
}