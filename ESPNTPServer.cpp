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
#include <lwip/def.h> // htonl() & ntohl()
#include "GPS.h"
#include "NTP.h"

//#include "Logger.h"

GPS gps(Serial, SYNC_PIN);
NTP ntp(gps);

#if defined(USE_OLED_DISPLAY)
SSD1306Wire    display(0x3c, SDA_PIN, SCL_PIN);
#endif

#include "Log.h"

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
    logger.info(SETUP_TAG, "\nStartup!");


#if !defined(USE_NO_WIFI)
    logger.info(SETUP_TAG, "initializing wifi");
    WiFiManager wifi;
    wifi.setDebugStream(Serial1);
    //wifi.setDebugOutput(false);
    String ssid = "SynchroClock" + String(ESP.getChipId());
    wifi.autoConnect(ssid.c_str(), NULL);
#endif

#if defined(USE_OLED_DISPLAY)
    logger.info(SETUP_TAG, "initializing display");
    if (!display.init())
    {
        logger.info(SETUP_TAG, "display.init() failed!");
    }
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
#endif

    logger.info(SETUP_TAG, "initializing serial for GPS");
    Serial.begin(9600);
    Serial.swap();

    logger.info(SETUP_TAG, "initializing GPS");
    gps.begin();

    logger.info(SETUP_TAG, "initializing NTP");
    ntp.begin();
}

void loop()
{
    static uint32_t min_loop;
    static uint32_t max_loop;
    static uint32_t last_loop;
    uint32_t start_loop = millis();

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
        if (tv.tv_sec != last_seconds && ((tv.tv_sec % 60) == 0 || gps.getValidDelay()))
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

#if defined(USE_OLED_DISPLAY)
        //
        // Update the display
        //
        display.clear();
        display.setTextAlignment(TEXT_ALIGN_LEFT);
        display.setFont(ArialMT_Plain_10);
        display.drawString(0, 0,  ts);
        display.drawString(0, 10, "Address:    "+WiFi.localIP().toString());
        display.drawString(0, 20, "Sat Count: " + String(gps.getSatelliteCount()));
        display.drawString(0, 30, "Requests:  " + String(ntp.getReqCount()));
        display.drawString(0, 40, "Responses: " + String(ntp.getRspCount()));
        snprintf(ts, 63, "loop: %d / %d / %d", last_loop, min_loop, max_loop);
        display.drawString(0, 50, String(ts));
        // write the buffer to the display
        display.display();
#endif
    }

    last_seconds = tv.tv_sec;
    last_loop    = millis() - start_loop;
    if (min_loop == 0 || last_loop < min_loop)
    {
        min_loop = last_loop;
    }
    if (max_loop == 0 || last_loop > max_loop)
    {
        max_loop = last_loop;
    }
}
