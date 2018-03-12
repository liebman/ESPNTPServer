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
#include "DLogPrintWriter.h"

#include "GPS.h"
#include "NTP.h"
#include "Display.h"

GPS gps(Serial, SYNC_PIN);
NTP ntp(gps);
Display display(gps, ntp, SDA_PIN, SCL_PIN);
DLog& dlog = DLog::getLog();

const char* SETUP_TAG = "setup";
const char* LOOP_TAG  = "loop";

void logTimeFirst(DLogBuffer& buffer, DLogLevel level)
{
    (void)level; // not used
    struct timeval tv;
    struct tm tm;
    gps.getTime(&tv);
    gmtime_r(&tv.tv_sec, &tm);

    buffer.printf(F("%04d/%02d/%02d %02d:%02d:%02d.%06ld "),
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

    //
    // Setup DLog with Serial1, set the pre function to add the date/time
    //
    Serial1.begin(115200);
    dlog.begin(new DLogPrintWriter(Serial1));
    dlog.setPreFunc(&logTimeFirst);

    dlog.info(SETUP_TAG, F("Startup!"));

    dlog.info(SETUP_TAG, F("initializing display"));
    display.begin();

    dlog.info(SETUP_TAG, F("initializing serial for GPS"));
    Serial.begin(9600);
    Serial.swap();

    dlog.info(SETUP_TAG, F("initializing GPS"));
    gps.begin();
    display.process();

#if !defined(USE_NO_WIFI)
    dlog.info(SETUP_TAG, F("initializing wifi"));
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

    dlog.info(SETUP_TAG, F("initializing NTP"));
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
        dlog.info(LOOP_TAG, F("wifi status change %d -> %d '%s'"), last_wifi_status, wifi_status, status);
        last_wifi_status = wifi_status;
    }

    IPAddress ip = WiFi.localIP();
    if (ip != last_ip)
    {
        dlog.warning(LOOP_TAG, F("ip address change %s -> %s"), last_ip.toString().c_str(), ip.toString().c_str());
        last_ip = ip;
    }

    gps.process();

    static time_t last_seconds;
    struct timeval tv;
    gps.getTime(&tv);

    if (tv.tv_sec != last_seconds)
    {
        if (tv.tv_sec != last_seconds && ((tv.tv_sec % 300) == 0 || gps.getValidDelay()))
        {
            dlog.info("loop", F("jitter:%lu valid_count:%lu valid:%s gpsvalid:%s numsat:%d heap:%ld valid_delay:%d"),
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
            dlog.warning(LOOP_TAG, F("OOPS: time went backwards: last:%lu now:%lu delta:%ld"), last_seconds, tv.tv_sec, tv.tv_sec-last_seconds);
        }

        display.process();
    }

    last_seconds = tv.tv_sec;
}
