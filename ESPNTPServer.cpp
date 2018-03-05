/*
 * ESPNTPServer.cpp
 *
 * Copyright 2017 Christopher B. Liebman
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 *  Created on: Oct 29, 2017
 *      Author: liebman
 */

#include "ESPNTPServer.h"
#include "Log.h"

#include "GPS.h"
#include "NTP.h"
#include "Display.h"

GPS gps(Serial, SYNC_PIN);
NTP ntp(gps);
Display display(gps, ntp, SDA_PIN, SCL_PIN);

const char* SETUP_TAG = "setup";
const char* LOOP_TAG  = "loop";

void logTimeFirst(Print* print)
{
    static struct timeval tv;
    gps.getTime(&tv);
    static struct tm tm;
    gmtime_r(&tv.tv_sec, &tm);

    print->printf("%04d/%02d/%02d %02d:%02d:%02d.%06ld ",
            tm.tm_year+1900,
            tm.tm_mon+1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            tm.tm_sec,
            tv.tv_usec);
}

void setup()
{
    delay(5000); // delay for IDE to re-open serial
    Serial1.begin(115200);
    logger.setSize(256);
    logger.setPrint(&Serial1);
    logger.setPreFunc(&logTimeFirst);
    logger.info(SETUP_TAG, "Startup!");

    logger.info(SETUP_TAG, "initializing display");
    display.begin();

    logger.info(SETUP_TAG, "initializing serial for GPS");
    Serial.begin(9600);
    Serial.swap();

    logger.info(SETUP_TAG, "initializing GPS");
    gps.begin();
    display.process();

#if !defined(USE_NO_WIFI)
    logger.info(SETUP_TAG, "initializing wifi");
#if 0
    WiFiManager wifi;
    wifi.setDebugStream(Serial1);
    wifi.setDebugOutput(false);
#else
    WiFiManager wifi(Serial1);
    wifi.setDebugOutput(true);
#endif
    wifi.setDebugOutput(true);
    String ssid = "ESPNTPServer" + String(ESP.getChipId());
    wifi.autoConnect(ssid.c_str(), NULL);
#endif

    display.process();

    logger.info(SETUP_TAG, "initializing NTP");
    ntp.begin();
}

void loop()
{
    static int last_wifi_status;
    static IPAddress last_ip;

    int wifi_status = WiFi.status();
    if (wifi_status != last_wifi_status)
    {
        const char* status;
        switch (wifi_status)
        {
            case WL_CONNECTED:
                status = "CONNECTED";
                break;
            case WL_NO_SSID_AVAIL:
                status = "NO_SSID";
                break;
            case WL_CONNECT_FAILED:
                status = "FAILED";
                break;
            case WL_IDLE_STATUS:
                status = "IDLE";
                break;
            case WL_DISCONNECTED:
                status = "DISCONNECTED";
                break;
            default:
                status = "<UNKNOWN>";
                break;
        }
        logger.info(LOOP_TAG, "wifi status change %d -> %d '%s'", last_wifi_status, wifi_status, status);
        last_wifi_status = wifi_status;
    }

    IPAddress ip = WiFi.localIP();
    if (ip != last_ip)
    {
        logger.warning(LOOP_TAG, "ip address change %s -> %s", last_ip.toString().c_str(), ip.toString().c_str());
        last_ip = ip;
    }

    gps.process();

    static time_t last_seconds;
    struct timeval tv;
    gps.getTime(&tv);
    if (tv.tv_sec != last_seconds)
    {
        struct tm tm;
        gmtime_r(&tv.tv_sec, &tm);
        char ts[64];
        snprintf(ts, 63, "%04d/%02d/%02d %02d:%02d:%02d.%06ld",
                tm.tm_year+1900,
                tm.tm_mon+1,
                tm.tm_mday,
                tm.tm_hour,
                tm.tm_min,
                tm.tm_sec,
                tv.tv_usec);
        if (tv.tv_sec != last_seconds && ((tv.tv_sec % 300) == 0 || gps.getValidDelay()))
        {
            logger.info("loop", "jitter:%lu valid_count:%lu valid:%s gpsvalid:%s numsat:%d heap:%ld valid_delay:%d",
                    gps.getJitter(),
                    gps.getValidCount(),
                    gps.isValid() ? "true" : "false",
                    gps.isGPSValid() ? "true" : "false",
                    gps.getSatelliteCount(),
                    ESP.getFreeHeap(),
                    gps.getValidDelay());
        }

        if (tv.tv_sec < last_seconds)
        {
            logger.warning(LOOP_TAG, "%s: OOPS: time went backwards: last:%lu now:%lu\n", ts, last_seconds, tv.tv_sec);
        }

        display.process();
    }

    last_seconds = tv.tv_sec;
}
