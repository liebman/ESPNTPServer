/*
 * TimeUtils.h
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
 *  Created on: Jun 7, 2017
 *      Author: liebman
 */

#ifndef TIMEUTILS_H_
#define TIMEUTILS_H_
#include <Arduino.h>
#include <time.h>
typedef struct
{
    int     tz_offset;     // seconds offset from UTC
    uint8_t month;         // starting month that this takes effect.
    int8_t  occurrence;    // 2 is second occurrence of the given day, -1 is last
    uint8_t day_of_week;   // 0 = Sunday
    uint8_t hour;
    int8_t  day_offset;    // +/- days (for Friday before last Sunday type)
} TimeChange;

class TimeUtils
{
public:
    static uint8_t    parseSmallDuration(const char* value);
    static int        parseOffset(const char* offset_string);
    static uint16_t   parsePosition(const char* position_string);
    static uint8_t    parseOccurrence(const char* occurrence_string);
    static uint8_t    parseDayOfWeek(const char* dow_string);
    static uint8_t    parseMonth(const char* month_string);
    static uint8_t    parseHour(const char* hour_string);
    static time_t     mktime(struct tm *tmbuf);
    static struct tm* gmtime_r(const time_t *timer, struct tm *tmbuf);
    static int        computeUTCOffset(time_t now, int tz_offset, TimeChange* tc, int tc_count);
    static uint8_t    findDOW(uint16_t y, uint8_t m, uint8_t d);
    static uint8_t    findNthDate(uint16_t year, uint8_t month, uint8_t dow, uint8_t nthWeek);
    static uint8_t    daysInMonth(uint16_t year, uint8_t month);
    static uint8_t    findDateForWeek(uint16_t year, uint8_t month, uint8_t dow, int8_t nthWeek);
};
#endif /* TIMEUTILS_H_ */
