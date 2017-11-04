/*
 * TimeUtils.cpp
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

#include "TimeUtils.h"

//#define DEBUG
#include "Logger.h"

uint8_t TimeUtils::parseSmallDuration(const char* value)
{
    int i = atoi(value);
    if (i < 0 || i > 255)
    {
        dbprintf("TimeUtils::parseSmallDuration: invalid value %s: using 32 instead!\n", value);
        i = 32;
    }
    return (uint8_t) i;
}

uint8_t TimeUtils::parseOccurrence(const char* occurrence_string)
{
    int i = atoi(occurrence_string);
    if (i < -5 || i == 0 || i > 5)
    {
        dbprintf("TimeUtils::parseOccurrence: invalid value %s: using 1 instead!\n", occurrence_string);
        i = 1;
    }
    return (uint8_t) i;
}

uint8_t TimeUtils::parseDayOfWeek(const char* dow_string)
{
    int i = atoi(dow_string);
    if (i < 0 || i > 6)
    {
        dbprintf("TimeUtils::parseDayOfWeek: invalid value %s: using 0 (Sunday) instead!\n", dow_string);
        i = 1;
    }
    return (uint8_t) i;
}

uint8_t TimeUtils::parseMonth(const char* month_string)
{
    int i = atoi(month_string);
    if (i < 0 || i > 12)
    {
        dbprintf("TimeUtils::parseMonth: invalid value '%s': using 3 (Mar) instead!\n", month_string);
        i = 1;
    }
    return (uint8_t) i;
}

uint8_t TimeUtils::parseHour(const char* hour_string)
{
    int i = atoi(hour_string);
    if (i < 0 || i > 23)
    {
        dbprintf("TimeUtils::parseMonth: invalid value '%s': using 2 instead!\n", hour_string);
        i = 1;
    }
    return (uint8_t) i;
}

int TimeUtils::parseOffset(const char* offset_string)
{
    int result = 0;
    char value[11];
    strncpy(value, offset_string, 10);
    if (strchr(value, ':') != NULL)
    {
        int sign = 1;
        char* s;

        if (value[0] == '-')
        {
            sign = -1;
            s = strtok(&(value[1]), ":");
        }
        else
        {
            s = strtok(value, ":");
        }
        if (s != NULL)
        {
            int h = atoi(s);
            while (h > 11)
            {
                h -= 12;
            }

            result += h * 3600; // hours to seconds
            s = strtok(NULL, ":");
        }
        if (s != NULL)
        {
            result += atoi(s) * 60; // minutes to seconds
            s = strtok(NULL, ":");
        }
        if (s != NULL)
        {
            result += atoi(s);
        }
        // apply sign
        result *= sign;
    }
    else
    {
        result = atoi(value);
        if (result < -43199 || result > 43199)
        {
            result = 0;
        }
    }
    return result;
}

uint16_t TimeUtils::parsePosition(const char* position_string)
{
    int result = 0;
    char value[10];
    strncpy(value, position_string, 9);
    if (strchr(value, ':') != NULL)
    {
        char* s = strtok(value, ":");

        if (s != NULL)
        {
            int h = atoi(s);
            while (h > 11)
            {
                h -= 12;
            }

            result += h * 3600; // hours to seconds
            s = strtok(NULL, ":");
        }
        if (s != NULL)
        {
            result += atoi(s) * 60; // minutes to seconds
            s = strtok(NULL, ":");
        }
        if (s != NULL)
        {
            result += atoi(s);
        }
    }
    else
    {
        result = atoi(value);
        if (result < 0 || result > 43199)
        {
            result = 0;
        }
    }
    return result;
}


//
// Modified code from: http://www.jbox.dk/sanos/source/lib/time.c.html
//

#define YEAR0                   1900
#define EPOCH_YR                1970
#define SECS_DAY                (24L * 60L * 60L)
#define LEAPYEAR(year)          (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEARSIZE(year)          (LEAPYEAR(year) ? 366 : 365)
#define TIME_MAX                2147483647L

const int _ytab[2][12] =
{
{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } };

// esp version is broken :-(
time_t TimeUtils::mktime(struct tm *tmbuf)
{
    long day, year;
    int tm_year;
    int yday, month;
    /*unsigned*/long seconds;
    int overflow;

    tmbuf->tm_min += tmbuf->tm_sec / 60;
    tmbuf->tm_sec %= 60;
    if (tmbuf->tm_sec < 0)
    {
        tmbuf->tm_sec += 60;
        tmbuf->tm_min--;
    }
    tmbuf->tm_hour += tmbuf->tm_min / 60;
    tmbuf->tm_min = tmbuf->tm_min % 60;
    if (tmbuf->tm_min < 0)
    {
        tmbuf->tm_min += 60;
        tmbuf->tm_hour--;
    }
    day = tmbuf->tm_hour / 24;
    tmbuf->tm_hour = tmbuf->tm_hour % 24;
    if (tmbuf->tm_hour < 0)
    {
        tmbuf->tm_hour += 24;
        day--;
    }
    tmbuf->tm_year += tmbuf->tm_mon / 12;
    tmbuf->tm_mon %= 12;
    if (tmbuf->tm_mon < 0)
    {
        tmbuf->tm_mon += 12;
        tmbuf->tm_year--;
    }
    day += (tmbuf->tm_mday - 1);
    while (day < 0)
    {
        if (--tmbuf->tm_mon < 0)
        {
            tmbuf->tm_year--;
            tmbuf->tm_mon = 11;
        }
        day += _ytab[LEAPYEAR(YEAR0 + tmbuf->tm_year)][tmbuf->tm_mon];
    }
    while (day >= _ytab[LEAPYEAR(YEAR0 + tmbuf->tm_year)][tmbuf->tm_mon])
    {
        day -= _ytab[LEAPYEAR(YEAR0 + tmbuf->tm_year)][tmbuf->tm_mon];
        if (++(tmbuf->tm_mon) == 12)
        {
            tmbuf->tm_mon = 0;
            tmbuf->tm_year++;
        }
    }
    tmbuf->tm_mday = day + 1;
    year = EPOCH_YR;
    if (tmbuf->tm_year < year - YEAR0)
        return (time_t) -1;
    seconds = 0;
    day = 0;                      // Means days since day 0 now
    overflow = 0;

    // Assume that when day becomes negative, there will certainly
    // be overflow on seconds.
    // The check for overflow needs not to be done for leapyears
    // divisible by 400.
    // The code only works when year (1970) is not a leapyear.
    tm_year = tmbuf->tm_year + YEAR0;

    if (TIME_MAX / 365 < tm_year - year)
        overflow++;
    day = (tm_year - year) * 365;
    if (TIME_MAX - day < (tm_year - year) / 4 + 1)
        overflow++;
    day += (tm_year - year) / 4 + ((tm_year % 4) && tm_year % 4 < year % 4);
    day -= (tm_year - year) / 100
            + ((tm_year % 100) && tm_year % 100 < year % 100);
    day += (tm_year - year) / 400
            + ((tm_year % 400) && tm_year % 400 < year % 400);

    yday = month = 0;
    while (month < tmbuf->tm_mon)
    {
        yday += _ytab[LEAPYEAR(tm_year)][month];
        month++;
    }
    yday += (tmbuf->tm_mday - 1);
    if (day + yday < 0)
        overflow++;
    day += yday;

    tmbuf->tm_yday = yday;
    tmbuf->tm_wday = (day + 4) % 7;               // Day 0 was thursday (4)

    seconds = ((tmbuf->tm_hour * 60L) + tmbuf->tm_min) * 60L + tmbuf->tm_sec;

    if ((TIME_MAX - seconds) / SECS_DAY < day)
        overflow++;
    seconds += day * SECS_DAY;

    if (overflow)
        return (time_t) -1;

    if ((time_t) seconds != seconds)
        return (time_t) -1;
    return (time_t) seconds;
}

struct tm *TimeUtils::gmtime_r(const time_t *timer, struct tm *tmbuf)
{
  time_t time = *timer;
  unsigned long dayclock, dayno;
  int year = EPOCH_YR;

  dayclock = (unsigned long) time % SECS_DAY;
  dayno = (unsigned long) time / SECS_DAY;

  tmbuf->tm_sec = dayclock % 60;
  tmbuf->tm_min = (dayclock % 3600) / 60;
  tmbuf->tm_hour = dayclock / 3600;
  tmbuf->tm_wday = (dayno + 4) % 7; // Day 0 was a thursday
  while (dayno >= (unsigned long) YEARSIZE(year)) {
    dayno -= YEARSIZE(year);
    year++;
  }
  tmbuf->tm_year = year - YEAR0;
  tmbuf->tm_yday = dayno;
  tmbuf->tm_mon = 0;
  while (dayno >= (unsigned long) _ytab[LEAPYEAR(year)][tmbuf->tm_mon]) {
    dayno -= _ytab[LEAPYEAR(year)][tmbuf->tm_mon];
    tmbuf->tm_mon++;
  }
  tmbuf->tm_mday = dayno + 1;
  tmbuf->tm_isdst = 0;
  return tmbuf;
}

//
// The functions findDOW & findNthDate are from:
//
//   http://hackaday.com/2012/07/16/automatic-daylight-savings-time-compensation-for-your-clock-projects
//

/*--------------------------------------------------------------------------
  FUNC: 6/11/11 - Returns day of week for any given date
  PARAMS: year, month, date
  RETURNS: day of week (0-7 is Sun-Sat)
  NOTES: Sakamoto's Algorithm
    http://en.wikipedia.org/wiki/Calculating_the_day_of_the_week#Sakamoto.27s_methods
    Altered to use char when possible to save microcontroller ram
--------------------------------------------------------------------------*/
uint8_t TimeUtils::findDOW(uint16_t y, uint8_t m, uint8_t d)
{
    static char t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    y -= m < 3;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

/*--------------------------------------------------------------------------
  http://hackaday.com/2012/07/16/automatic-daylight-savings-time-compensation-for-your-clock-projects
  FUNC: 6/11/11 - Returns the date for Nth day of month. For instance,
    it will return the numeric date for the 2nd Sunday of April
  PARAMS: year, month, day of week, Nth occurrence of that day in that month
  RETURNS: date
  NOTES: There is no error checking for invalid inputs.
--------------------------------------------------------------------------*/
uint8_t TimeUtils::findNthDate(uint16_t year, uint8_t month, uint8_t dow, uint8_t nthWeek)
{
    dbprintf("findNthDate: year:%u month:%u, dow:%u nthWeek:%d\n", year, month, dow, nthWeek);

    uint8_t targetDate = 1;
    uint8_t firstDOW = findDOW(year,month,targetDate);
    while (firstDOW != dow) {
        firstDOW = (firstDOW+1)%7;
        targetDate++;
    }
    //Adjust for weeks
    targetDate += (nthWeek-1)*7;
    return targetDate;
}

uint8_t TimeUtils::daysInMonth(uint16_t year, uint8_t month)
{
    uint8_t days = 31;

    switch(month)
    {
     case 2:
         days = 28;
         if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
         {
             days = 29;
         }
         break;
     case 4:
     case 6:
     case 9:
     case 11:
         days = 30;
         break;
    }
    return days;
}

uint8_t TimeUtils::findDateForWeek(uint16_t year, uint8_t month, uint8_t dow, int8_t week)
{
    dbprintf("findDateForWeek: year:%u month:%u, dow:%u week:%d\n", year, month, dow, week);

    uint8_t weeks[5];
    uint8_t max_day = daysInMonth(year, month);
    int last = 0;

    if (week >= 0)
    {
        return findNthDate(year, month, dow, week);
    }

    //
    // find all times this weekday shows up in the month
    // Note that 'last' will end up pointing 1 past the last
    // valid occurrence.  -1 will give the last one.
    //
    for(last = 0; last <= 5; ++last)
    {
        weeks[last] = findNthDate(year, month, dow, last+1);

        dbprintf("findDateForWeek: last:%d date:%u\n", last, weeks[last]);

        if (weeks[last] > max_day)
        {
            break;
        }
    }

    return weeks[last+week];
}

int TimeUtils::computeUTCOffset(time_t now, int tz_offset, TimeChange* tc, int tc_count)
{
    struct tm tm;

    //
    // get the current year
    //
    gmtime_r(&now, &tm);
    int year = tm.tm_year;

    //
    // pre-set the offset to the last timechange of the year
    //
    int offset = tc[tc_count-1].tz_offset;

    //
    // loop thru each time change entry, converting it to the time in seconds for the
    // current year.  If now is greater/equal to the time change the use the new offset.
    // We return the last offset that is greater/equal now.
    //
    for(int i = 0; i < tc_count; ++i)
    {
        dbprintf("TimeUtils::computeUTCOffset: index:%d offset:%d month:%u dow:%u occurrence:%d hour:%u day_offset:%d\n",
                i,
                tc[i].tz_offset,
                tc[i].month,
                tc[i].day_of_week,
                tc[i].occurrence,
                tc[i].hour,
                tc[i].day_offset);

        tm.tm_sec    = 0;
        tm.tm_min    = 0;
        tm.tm_hour   = tc[i].hour;
        tm.tm_mday   = TimeUtils::findDateForWeek(year+1900, tc[i].month, tc[i].day_of_week, tc[i].occurrence);
        tm.tm_mon    = tc[i].month-1;
        tm.tm_year   = year;

        dbprintf("computeUTCOffset: tm: %04d/%02d/%02d %02d:%02d:%02d + %d days\n", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tc[i].day_offset);

        // convert to seconds
        time_t tc_time = mktime(&tm);
        // convert to UTC
        tc_time -= tz_offset;
        dbprintf("computeUTCOffset: tc_time: %ld (UTC)\n", tc_time);
        // add in days offset
        tc_time += tc[i].day_offset*86400;

        dbprintf("computeUTCOffset: now: %ld tc_time: %ld\n", now, tc_time);

        if (now >= tc_time)
        {
            offset = tc[i].tz_offset;
            dbprintf("computeUTCOffset: now > tc_time, offset: %d\n", offset);
        }
    }

    return offset;
}
