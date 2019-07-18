/*
 * GPS.h
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
 *  Created on: Feb 26, 2018
 *      Author: chris.l
 */

#ifndef GPS_H_
#define GPS_H_
#include "Arduino.h"
#include "MicroNMEA.h"
#include "Ticker.h"

#define REASON_SIZE       128
#define NMEA_BUFFER_SIZE  250
#define PPS_TIMING_PIN    12   // (GPIO12) if defined PPS interrupt will make high during processing
#define VALID_DELAY       120  // delay (seconds) from gps valid to valid
#define VALID_TIMER_MS    1001 // if this timer expires we invalidate!
#define NMEA_TIMER_MS     1100 // if this timer expires we mark NMEA time late

#define MICROS_PER_SEC         1000000

#define us2s(x) (((double)x)/(double)MICROS_PER_SEC) // microseconds to seconds

//  simple versions - we don't worry about side effects
#define MAX(a, b)   ((a) < (b) ? (b) : (a))
#define MIN(a, b)   ((a) < (b) ? (a) : (b))

#if defined(PPS_TIMING_PIN)
#define PPS_TIMIMG_PIN_INIT() {digitalWrite(PPS_TIMING_PIN, LOW); pinMode(PPS_TIMING_PIN, OUTPUT);}
#define PPS_TIMING_PIN_ON()   digitalWrite(PPS_TIMING_PIN, HIGH)
#define PPS_TIMING_PIN_OFF()  digitalWrite(PPS_TIMING_PIN, LOW)
#else
#define PPS_TIMIMG_PIN_INIT()
#define PPS_TIMING_PIN_ON()
#define PPS_TIMING_PIN_OFF()
#endif

class GPS
{
public:
    GPS(Stream& gps_serial, int pps_pin);
    virtual ~GPS();

    void     begin();
    void     process();
    void     end();

    bool     isValid()       { return _valid; }
    bool     isGPSValid()    { return _gps_valid; }
    uint32_t getJitter()     { return _max_micros - _min_micros; }
    uint32_t getValidCount() { return _valid_count; }
    uint32_t getValidDelay() { return _valid_delay; }
    time_t   getValidSince() { return _valid_since; }
    uint8_t  getSatelliteCount() { return _nmea.getNumSatellites(); }
    time_t   getSeconds()        { return _seconds; }
    void     getTime(struct timeval* tv);
    double   getDispersion();

    // we don't allow copying this guy!
    GPS(const GPS&)            = delete;
    GPS& operator=(const GPS&) = delete;

private:
    Stream&           _stream;
    char              _buffer[NMEA_BUFFER_SIZE];
    MicroNMEA         _nmea;
    Ticker            _pps_timer;
    Ticker            _nmea_timer;
    std::function<void()> _invalidate;
    std::function<void()> _nmea_timeout;
    volatile time_t   _seconds;
    volatile uint32_t _valid_delay;  // delay (seconds) from gps_valid until we thing we are valid
    volatile uint32_t _valid_count;  // number of times we have gone valid
    volatile time_t   _valid_since;
    volatile uint32_t _min_micros;
    volatile uint32_t _max_micros;
    volatile uint32_t _last_micros;
    volatile uint32_t _timeouts;

    uint8_t           _pps_pin;
    bool              _gps_valid;
    volatile bool     _valid;
    volatile bool     _nmea_late;
    char              _reason[REASON_SIZE];
    void pps();        // interrupt handler
    void timeout();
    void invalidate(const char* fmt, ...);
    void nmeaTimeout();
};

#endif /* GPS_H_ */
