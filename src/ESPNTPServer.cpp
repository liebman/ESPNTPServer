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
#include "ESPNTPServerVersion.h"

#include "Log.h"
#include "DLogPrintWriter.h"
#include "DLogSyslogWriter.h"

#include "GPS.h"
#include "NTP.h"
#include "Display.h"
#include "Config.h"

DLog& dlog = DLog::getLog();
GPS gps(Serial, SYNC_PIN);
NTP ntp(gps);
Display display(gps, ntp, SDA_PIN, SCL_PIN);
Config config;

char devicename[32];

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

void processOTA(const char* ota_url, const char* ota_fp)
{
    static PROGMEM const char TAG[] = "processOTA";
    dlog.info(FPSTR(TAG), F("OTA update"));

	dlog.info(FPSTR(TAG), F("checking for OTA from: '%s'"), ota_url);
	display.message("Starting OTA");

	ESPhttpUpdate.rebootOnUpdate(false);

#ifdef USE_CERT_STORE
    CertStore certStore;
#endif
    WiFiClient *client = nullptr;

    if (strncmp(ota_url, "https:", 6) == 0)
    {
#ifdef USE_CERT_STORE
        int numCerts = certStore.initCertStore(SPIFFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
        dlog.info(FPSTR(TAG), F("Number of CA certs read: %d"), numCerts);
#endif
        BearSSL::WiFiClientSecure *bear  = new BearSSL::WiFiClientSecure();
#ifdef USE_CERT_STORE
        // Integrate the cert store with this connection
        dlog.info(FPSTR(TAG), F("adding cert store to connection"));
        bear->setCertStore(&certStore);
#else
        bear->setInsecure();
#endif
        client = bear;
    }
    else
    {
        client = new WiFiClient;
    }

    t_httpUpdate_return ret = ESPhttpUpdate.update(*client, ota_url, "0.4");

	String reason = ESPhttpUpdate.getLastErrorString();

	switch(ret)
	{
		case HTTP_UPDATE_OK:
			dlog.info(FPSTR(TAG), F("OTA update OK! restarting..."));
			dlog.end();
			display.message("Success!");
			delay(10000);
			ESP.restart();
			break;

		case HTTP_UPDATE_FAILED:
			dlog.info(FPSTR(TAG), F("OTA update failed! reason:'%s'"), reason.c_str());
			display.message("FAILED!");
			delay(10000);
			break;

		case HTTP_UPDATE_NO_UPDATES:
			dlog.info(FPSTR(TAG), F("OTA no updates! reason:'%s'"), reason.c_str());
			display.message("NO Update!");
			delay(10000);
			break;

		default:
			dlog.info(FPSTR(TAG), F("OTA update WTF? unexpected return code: %d reason: '%s'"), ret, reason.c_str());
			display.message("Unknown!");
			delay(10000);
			break;
	}
}

void setup()
{
    //delay(5000); // delay for IDE to re-open serial

    //
    // Setup DLog with Serial1, set the pre function to add the date/time
    //
    Serial1.begin(76800);
    dlog.begin(new DLogPrintWriter(Serial1));
    dlog.setPreFunc(&logTimeFirst);
    //dlog.setLevel("GPS", DLogLevel::DLOG_LEVEL_DEBUG);
    dlog.setLevel("Config", DLogLevel::DLOG_LEVEL_DEBUG);

    dlog.info(SETUP_TAG, F("Startup!"));

    snprintf(devicename, sizeof(devicename), "ESPNTP:%08x", ESP.getChipId());
    dlog.info(SETUP_TAG, "Device name: %s", devicename);

    dlog.info(SETUP_TAG, F("initializing display"));
    display.begin();


    dlog.info(SETUP_TAG, F("initializing serial for GPS"));
    Serial.begin(9600);
    Serial.swap();

    dlog.info(SETUP_TAG, F("initializing GPS"));
    display.message("Starting GPS");
    gps.begin();
    display.process();

    bool force_config = false;

    config.begin();

    if (!config.load())
    {
        dlog.warning(SETUP_TAG, "no config found, forcing config portal!");
        force_config = true;
    }

    // if the reset/config button is pressed then force config
    if (digitalRead(CONFIG_PIN) == 0)
    {
        time_t start = millis();

        while (digitalRead(CONFIG_PIN) == 0)
        {
            time_t delta = millis() - start;
			if (!force_config && delta > CONFIG_DELAY)
			{
                dlog.info(SETUP_TAG, F("reset button held, forcing config!"));
                force_config = true;
				display.message("Force Config");
			}
        }
    }

#if !defined(USE_NO_WIFI)
    dlog.info(SETUP_TAG, F("initializing wifi"));
    display.message("Starting WiFi");
    WiFiSetup wifi(config, display, Serial1, false, devicename);
    wifi.connect(force_config);

    const char* syslog_host = config.getSyslogHost();
    uint16_t    syslog_port = config.getSyslogPort();
    if (syslog_host != nullptr && strlen(syslog_host) > 0 && syslog_port != 0)
    {
        dlog.info(SETUP_TAG, "enabling syslog: '%s:%u'", syslog_host, syslog_port);
        dlog.begin(new DLogSyslogWriter(syslog_host, syslog_port, devicename, ESPNTP_SERVER_VERSION));
    }
    const char* url = wifi.getOTAURL();
    const char* fp  = wifi.getOTAFP();
    if (fp == nullptr)
    {
    	fp = "";
    }

    if (url != nullptr)
    {
    	processOTA(url, fp);
    }

    WiFiMode_t mode = WiFi.getMode();
    dlog.error(SETUP_TAG, "WiFi mode: %d", mode);
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

#if 0
    uint32_t m = millis();
    if (m % 1000 == 0)
    {
    	dlog.info("loop", "uptime: %lu millis: %lu", m);
    }
#endif

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
