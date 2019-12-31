/*
 * WiFiSetup.cpp
 *
 * Copyright 2018 Christopher B. Liebman
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
 *  Created on: May 27, 2018
 *      Author: chris.l
 */

#include "WiFiSetup.h"
#include <functional>

#include "Log.h"
static const char* TAG = "WiFiSetup";

WiFiSetup::WiFiSetup(Config& config, Display& display, Stream& serial, boolean debug, const char* devicename)
: _config(config),
  _display(display),
  _wm(serial),
  _ota_url("ota_url", "OTA URL",         "", 128),
  _ota_fp("ota_fp",   "OTA Fingerprint", "", 64),
  _syslog_host("syslog_host", "Syslog Host", "", 64),
  _syslog_port("syslog_port", "Syslog Port", "514", 8),
  _devicename(devicename)
{
    _wm.setDebugOutput(debug);

    using namespace std::placeholders;
    _wm.setAPCallback(std::bind(&WiFiSetup::startingPortal, this, _1));
    _wm.setSaveConfigCallback(std::bind(&WiFiSetup::saveConfig, this));
}

WiFiSetup::~WiFiSetup()
{
}

void WiFiSetup::connect(bool force_config)
{
    WiFi.mode(WiFiMode::WIFI_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);

    dlog.info(TAG, F("connect: disableing captive portal when auto-connecting"));
    _wm.setEnableConfigPortal(false); // don't automatically use the captive portal

    if (force_config)
    {
        dlog.info(TAG, F("connect: starting forced config portal!"));
        _wm.startConfigPortal(_devicename, NULL);
    }
    else
    {
        dlog.info(TAG, F("connect: auto-connecting"));
        while (!_wm.autoConnect(_devicename, NULL))
        {
            dlog.error(TAG, F("connect: not connected! retrying...."));
            delay(1000);
        }
    }
}

void WiFiSetup::startingPortal(WiFiManager* wmp)
{
    (void) wmp;
    dlog.info(TAG, F("startingPortal: updating param values"));
    _syslog_host.setValue(_config.getSyslogHost(), 64);
    char value[10];
    snprintf(value, sizeof(value), "%u", _config.getSyslogPort());
    _syslog_port.setValue(value, 8);
    dlog.info(TAG, F("startingPortal: adding params"));
    _wm.addParameter(&_ota_url);
    _wm.addParameter(&_ota_fp);
    _wm.addParameter(&_syslog_host);
    _wm.addParameter(&_syslog_port);

    dlog.debug(TAG, F("startingPortal: params added!"));

    //
    // Update the display with the SSID to show portal is up
    //
    dlog.info(TAG, F("startingPortal: updating display"));
    _display.message("SSID: %s", _wm.getConfigPortalSSID().c_str());
}

void WiFiSetup::saveConfig()
{
    dlog.info(TAG, "saveConfig: updating values");
    _config.setSyslogHost(_syslog_host.getValue());
    _config.setSyslogPort(atoi(_syslog_port.getValue()));
    dlog.debug(TAG, "saveConfig: saving!");
    _config.save();
}

const char* WiFiSetup::getParam(WiFiManagerParameter& param)
{
    const char* value = param.getValue();

    if (strlen(value) > 0)
    {
        return value;
    }

    return NULL;
}

const char* WiFiSetup::getOTAURL()
{
    return getParam(_ota_url);
}

const char* WiFiSetup::getOTAFP()
{
    return getParam(_ota_fp);
}

const char* WiFiSetup::getSyslogHost()
{
    return getParam(_syslog_host);
}

uint16_t WiFiSetup::getSyslogPort()
{
    const char* value = getParam(_syslog_port);
    int port = atoi(value);
    return port;
}
