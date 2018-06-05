/*
 * WiFiSetup.h
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

#ifndef WIFISETUP_H_
#define WIFISETUP_H_

#include "Arduino.h"
#include "WiFiManager.h"
#include "Display.h"

class WiFiSetup {
public:
	WiFiSetup(Display& display, Stream& serial, boolean debug);
	virtual ~WiFiSetup();
	void connect(bool force_config = false);
	const char* getOTAURL();
	const char* getOTAFP();
private:
    Display&             _display;
	WiFiManager          _wm;
	WiFiManagerParameter _ota_url;
	WiFiManagerParameter _ota_fp;
	void startingPortal(WiFiManager* wmp);
	const char*getParam(WiFiManagerParameter& param);

};

#endif /* WIFISETUP_H_ */
