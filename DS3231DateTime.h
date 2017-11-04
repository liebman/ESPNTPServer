/*
 * DS3231DateTime.h
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
 *  Created on: May 22, 2017
 *      Author: chris.l
 */

#ifndef DS3231DATETIME_H_
#define DS3231DATETIME_H_
#include <Arduino.h>
#include "TimeUtils.h"

#define MAX_POSITION      43200 // seconds in 12 hours

class DS3231DateTime
{
public:
	DS3231DateTime();
	boolean       isValid();
	void          setUnixTime(unsigned long time);
	unsigned long getUnixTime();
	void          applyOffset(int offset);
	uint16_t      getPosition();
    uint16_t      getPosition(int offset);
    const char*   string();

	uint8_t  getDay();
    uint8_t  getDate();
	uint8_t  getHour();
    uint8_t  getMonth();
    uint16_t getYear();

	friend class DS3231;
protected:
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t date;
	uint8_t day;
	uint8_t month;
	uint8_t year;
    uint8_t century;
};

#endif /* DS3231DATETIME_H_ */
