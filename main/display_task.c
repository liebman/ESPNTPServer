/*
 * display_task.c
 *
 *  Created on: Jan 21, 2018
 *      Author: chris.l
 */

#include "esp_gps_ntp.h"
#include "freertos/queue.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include <time.h>
#include <string.h>
#include "tcpip_adapter.h"

static const char *TAG = "DSP";
static xQueueHandle dsp_queue = NULL;
static u8g2_t dsp;

void IRAM_ATTR triggerDisplay()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool value = true;

    if (dsp_queue != NULL)
    {
        xQueueSendFromISR(dsp_queue, &value, &xHigherPriorityTaskWoken );
    }
}

static void dsp_task()
{
    GPSTaskStatus gps;
    NTPTaskStatus ntp;
    static long unsigned int duration;
    static bool blink;
    const int buf_size = 64;
    char buf[buf_size];
    struct tm t;
    tcpip_adapter_ip_info_t ip;
    memset(&ip, 0, sizeof(tcpip_adapter_ip_info_t));
    bool value;
    ESP_LOGI(TAG, "starting!");

    dsp_queue = xQueueCreate(10, sizeof(value));

    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.sda = SDA_PIN;
    u8g2_esp32_hal.scl = SCL_PIN;
    u8g2_esp32_hal_init(u8g2_esp32_hal);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&dsp, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);  // init u8g2 structure
    u8g2_SetI2CAddress(&dsp, 0x3c<<1);
    ESP_LOGD(TAG, "initializing display");
    u8g2_InitDisplay(&dsp); // send init sequence to the display, display is in sleep mode after this,
    ESP_LOGD(TAG, "waking display");
    u8g2_ClearDisplay(&dsp);
    u8g2_SetPowerSave(&dsp, 0); // wake up display
    u8g2_SetFont(&dsp, u8g2_font_timR08_tr);
    u8g2_SetFontMode(&dsp, 0);

    while(1)
    {
        if (xQueueReceive(dsp_queue, &value, pdMS_TO_TICKS(10000)))
        {
            micros_t start = micros();
            getGPSTaskStatus(&gps);
            getNTPTaskStatus(&ntp);
            u8g2_ClearBuffer(&dsp);
            gmtime_r(&gps.seconds, &t);
            snprintf(buf, buf_size-1, "UTC: %04d/%02d/%02d %02d:%02d:%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
            u8g2_DrawStr(&dsp,  0,  8, buf);
            snprintf(buf, buf_size-1, "Req: %u", ntp.req_count);
            u8g2_DrawStr(&dsp,  0, 16, buf);
            snprintf(buf, buf_size-1, "Rsp: %u", ntp.rsp_count);
            u8g2_DrawStr(&dsp, 64, 16, buf);
            snprintf(buf, buf_size-1, "Sat: %d", gps.sat_count);
            u8g2_DrawStr(&dsp,  0, 24, buf);
            snprintf(buf, buf_size-1, "Fix: %d", gps.fix_quality);
            u8g2_DrawStr(&dsp, 64, 24, buf);
            snprintf(buf, buf_size, "Valid: %d", gps.valid_count);
            u8g2_DrawStr(&dsp,  0, 32, buf);
            snprintf(buf, buf_size, "Timeout: %d", gps.timeouts);
            u8g2_DrawStr(&dsp, 64, 32, buf);

            if (gps.valid_in)
            {
                snprintf(buf, buf_size, "Valid In: %d", gps.valid_in);
                u8g2_DrawStr(&dsp,  0, 40, buf);
            }

            gmtime_r(&gps.valid_since, &t);
            if (gps.valid)
            {
                snprintf(buf, buf_size-1, "Valid: %04d/%02d/%02d %02d:%02d:%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
                u8g2_DrawStr(&dsp, 0, 48, buf);
            }
            else
            {
                if (blink)
                {
                    snprintf(buf, buf_size-1, "Invld: %04d/%02d/%02d %02d:%02d:%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
                    u8g2_DrawStr(&dsp, 0, 48, buf);
                }
            }
            gmtime_r(&gps.initial_valid, &t);
            snprintf(buf, buf_size-1, "Start: %04d/%02d/%02d %02d:%02d:%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
            u8g2_DrawStr(&dsp, 0, 56, buf);
            if (tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA, &ip) == 0) {
                snprintf(buf, buf_size-1, "IP: "IPSTR, IP2STR(&ip.ip));
                u8g2_DrawStr(&dsp, 0, 64, buf);
            }
            u8g2_SendBuffer(&dsp);
            micros_t end = micros();
            duration = end - start;
            blink = !blink;
        }
        else
        {
            ESP_LOGD(TAG, "timeout waiting for display queue!");
        }
    }
}


void start_display()
{
    esp_log_level_set(TAG,        CONFIG_DISPLAY_LOG_LEVEL);
    esp_log_level_set("u8g2_hal", CONFIG_U8G2_HAL_LOG_LEVEL);

    xTaskCreatePinnedToCore(dsp_task, "dsp_task", 1024*4, NULL, DSP_TASK_PRI, NULL, 1);

}
