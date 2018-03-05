/*
 * ESPNTPServer.h
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

//#define USE_NO_WIFI

#ifndef _ESPNTPServer_H_
#define _ESPNTPServer_H_
#include "Arduino.h"
#include "Ticker.h"
#if !defined(USE_NO_WIFI)
#include "WiFiManager.h"
#endif
#include <time.h>

// pin definitions
#define SYNC_PIN               14   // (GPIO14) pin tied to 1hz square wave from GPS
#define SDA_PIN                4
#define SCL_PIN                5


#endif /* _ESPNTPServer_H_ */
