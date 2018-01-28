/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_gps_ntp.h"
#include <stdio.h>
#include "esp_system.h"

#include "esp_wifi.h"
#include "esp_wps.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"


/*set wps mode via "make menuconfig"*/
#if CONFIG_WPS_TYPE_PBC
#define WPS_TEST_MODE WPS_TYPE_PBC
#elif CONFIG_WPS_TYPE_PIN
#define WPS_TEST_MODE WPS_TYPE_PIN
#else
#define WPS_TEST_MODE WPS_TYPE_DISABLE
#endif /*CONFIG_WPS_TYPE_PBC*/


#ifndef PIN2STR
#define PIN2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
#define PINSTR "%c%c%c%c%c%c%c%c"
#endif


static const char *TAG = "ESP_GPS_NTP";

static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TEST_MODE);

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            ESP_LOGI(TAG, "got ip:%s\n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            start_ntp();
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;

        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
            /*point: the function esp_wifi_wps_start() only get ssid & password
             * so call the function esp_wifi_connect() here
             * */
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;

        case SYSTEM_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED");
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
            ESP_ERROR_CHECK(esp_wifi_wps_start(0));
            break;

        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT");
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
            ESP_ERROR_CHECK(esp_wifi_wps_start(0));
            break;

        case SYSTEM_EVENT_STA_WPS_ER_PIN:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN");
            /*show the PIN code here*/
            ESP_LOGI(TAG, "WPS_PIN = "PINSTR, PIN2STR(event->event_info.sta_er_pin.pin_code));
            break;

        default:
            break;
    }
    return ESP_OK;
}

/*init wifi as sta and start wps*/
static void start_wifi(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    if (esp_wifi_connect())
    {
        ESP_LOGI(TAG, "wifi not configured, starting wps...");


        ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
    }
}

micros_t IRAM_ATTR micros()
{
    return esp_timer_get_time();
}

void app_main()
{
    esp_log_level_set(TAG,        CONFIG_APP_LOG_LEVEL);
    ESP_LOGI(TAG, "ESP32 GPS NTP Server");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

    ESP_LOGI(TAG, "***** RUNNING on core %d *****", xPortGetCoreID());
    fflush(stdout);

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    start_wifi();
    start_gps();
    start_display();
}
