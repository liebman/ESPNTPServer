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

WiFiSetup::WiFiSetup(Display& display, Stream& serial, boolean debug)
: _display(display),
  _wm(serial),
  _ota_url("ota_url", "OTA URL",         "", 128),
  _ota_fp("ota_fp",   "OTA Fingerprint", "", 64)
{
	_wm.setDebugOutput(debug);

	using namespace std::placeholders;
	_wm.setAPCallback(std::bind(&WiFiSetup::startingPortal, this, _1));
}

WiFiSetup::~WiFiSetup() {
}

void WiFiSetup::connect(bool force_config)
{
    String ssid = "ESPNTPServer" + String(ESP.getChipId());

	if (force_config)
	{
	    dlog.info(TAG, F("connect: starting forced config portal!"));
        _wm.startConfigPortal(ssid.c_str(), NULL);
	}
	else
	{
	    dlog.info(TAG, F("connect: auto-connecting"));
        _wm.autoConnect(ssid.c_str(), NULL);
	}
}

void WiFiSetup::startingPortal(WiFiManager* wmp)
{
	(void)wmp;
	dlog.info(TAG, F("startingPortal: adding params"));
	_wm.addParameter(&_ota_url);
	_wm.addParameter(&_ota_fp);
    dlog.debug(TAG, F("startingPortal: params added!"));

    //
    // Update the display with the SSID to show portal is up
    //
    dlog.info(TAG, F("startingPortal: updating display"));
    _display.message("SSID: %s", _wm.getConfigPortalSSID().c_str());
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
