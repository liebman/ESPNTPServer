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
#include <lwip/def.h> // htonl() & ntohl()
#define DEBUG
#include "Logger.h"

//define NTP_PACKET_DEBUG

int8_t            precision;
volatile uint32_t dispersion;

volatile uint32_t seconds;

volatile uint32_t last_micros;
volatile uint32_t micros_wraps;
volatile uint32_t min_micros;
volatile uint32_t max_micros;

#if defined(MICROS_HISTORY_SIZE)
volatile uint32_t micros_history[MICROS_HISTORY_SIZE];
volatile uint16_t micros_history_count;
volatile uint16_t micros_history_index;
#endif


AsyncUDP udp;
DS3231   rtc;                       // real time clock on i2c interface

#ifdef NTP_PACKET_DEBUG
void dumpNTPPacket(NTPPacket* ntp)
{
    dbprintf("size:       %u\n", sizeof(*ntp));
    dbprintf("firstbyte:  0x%02x\n", *(uint8_t*)ntp);
    dbprintf("li:         %u\n", getLI(ntp->flags));
    dbprintf("version:    %u\n", getVERS(ntp->flags));
    dbprintf("mode:       %u\n", getMODE(ntp->flags));
    dbprintf("stratum:    %u\n", ntp->stratum);
    dbprintf("poll:       %u\n", ntp->poll);
    dbprintf("precision:  %d\n", ntp->precision);
    dbprintf("delay:      %u\n", ntp->delay);
    dbprintf("dispersion: %u\n", ntp->dispersion);
    dbprintf("ref_id:     %02x:%02x:%02x:%02x\n", ntp->ref_id[0], ntp->ref_id[1], ntp->ref_id[2], ntp->ref_id[3]);
    dbprintf("ref_time:   %08x:%08x\n", ntp->ref_time.seconds, ntp->ref_time.fraction);
    dbprintf("orig_time:  %08x:%08x\n", ntp->orig_time.seconds, ntp->orig_time.fraction);
    dbprintf("recv_time:  %08x:%08x\n", ntp->recv_time.seconds, ntp->recv_time.fraction);
    dbprintf("xmit_time:  %08x:%08x\n", ntp->xmit_time.seconds, ntp->xmit_time.fraction);
}
#else
#define dumpNTPPacket(x)
#endif

void oneSecondInterrupt()
{
    uint32_t cur_micros = micros();
    //
    // the first time around we just initialize the last value
    //
    if (last_micros == 0)
    {
        last_micros = cur_micros;
        return;
    }

    if (cur_micros < last_micros)
    {
        ++micros_wraps;
    }

    uint32_t micros_count = cur_micros - last_micros;
    last_micros           = cur_micros;

    if (min_micros == 0 || micros_count < min_micros)
    {
        min_micros = micros_count;
    }

    if (micros_count > max_micros)
    {
        max_micros = micros_count;
    }

#if defined(MICROS_HISTORY_SIZE)
    micros_history[micros_history_index++] = micros_count;
    if (micros_history_index >= MICROS_HISTORY_SIZE)
    {
        micros_history_index = 0;
    }
    if (micros_history_count < MICROS_HISTORY_SIZE)
    {
        micros_history_count++;
    }
#endif

    //
    // increment seconds
    //
    seconds += 1;

#if defined(DEBUG)
    digitalWrite(LED_PIN, digitalRead(LED_PIN) ? LOW : HIGH);
#endif
}

void waitForEdge(int edge)
{
    while (digitalRead(SYNC_PIN) == edge)
    {
        delay(0);
    }
    while (digitalRead(SYNC_PIN) != edge)
    {
        delay(0);
    }
}

void getNTPTime(NTPTime *time)
{
    time->seconds         = toNTP(seconds);
    uint32_t cur_micros   = micros();
    uint32_t micros_delta = cur_micros - last_micros;

    //
    // if micros_delta is at or bigger than one second then
    // use the max fraction.
    //
    if (micros_delta >= 1000000)
    {
        time->fraction = 0xffffffff;
        return;
    }

    double percent      = us2s(micros_delta);
    //dbprintf("micros_delta: %lu percent: %lf\n", micros_delta, percent);
    time->fraction      = (uint32_t)(percent * (double)4294967296L);
}

int8_t computePrecision()
{
    NTPTime t;
    unsigned long start = micros();
    for (int i = 0; i < PRECISION_COUNT; ++i)
    {
        getNTPTime(&t);
    }
    unsigned long end = micros();
    double total      = (double)(end - start) / 1000000.0;
    double time       = total / PRECISION_COUNT;
    double prec       = log2(time);
    dbprintf("computePrecision: total:%f time:%f prec:%f\n", total, time, prec);
    return (int8_t)prec;
}

int updateSeconds()
{
    DS3231DateTime dt;
    if (rtc.readTime(dt))
    {
        dbprintln("updateSeconds: FAILED to read RTC, clearing bus & not checking seconds!");
        WireUtils.clearBus();
        return -1;
    }

    uint32_t old_seconds = seconds;
    uint32_t now = dt.getUnixTime();
    if (now != seconds)
    {
        seconds = now;
        dbprintf("updateSeconds: updated seconds from %lu to %lu\n", old_seconds, now);
    }

    return 0;
}

void recievePacket(AsyncUDPPacket aup)
{
    NTPTime recv_time;
    getNTPTime(&recv_time);

    if (aup.length() != sizeof(NTPPacket))
    {
        dbprintf("recievePacket: ignoring packet with bad length: %d < %d\n", aup.length(), sizeof(NTPPacket));
        return;
    }
    NTPPacket ntp;
    memcpy(&ntp, aup.data(), sizeof(NTPPacket));
    ntp.delay              = ntohl(ntp.delay);
    ntp.dispersion         = ntohl(ntp.dispersion);
    ntp.orig_time.seconds  = ntohl(ntp.orig_time.seconds);
    ntp.orig_time.fraction = ntohl(ntp.orig_time.fraction);
    ntp.ref_time.seconds   = ntohl(ntp.ref_time.seconds);
    ntp.ref_time.fraction  = ntohl(ntp.ref_time.fraction);
    ntp.recv_time.seconds  = ntohl(ntp.recv_time.seconds);
    ntp.recv_time.fraction = ntohl(ntp.recv_time.fraction);
    ntp.xmit_time.seconds  = ntohl(ntp.xmit_time.seconds);
    ntp.xmit_time.fraction = ntohl(ntp.xmit_time.fraction);
    dumpNTPPacket(&ntp);

    //
    // Build the response
    //
    ntp.flags = setLI(LI_NONE) | setVERS(NTP_VERSION) | setMODE(MODE_SERVER);
    ntp.stratum = 1;
    ntp.precision = precision;
    // TODO: compute actual root delay, and root dispersion
    ntp.delay      = (uint32)(0.000001 * 65536);
    ntp.dispersion = dispersion;
    strncpy((char*)ntp.ref_id, REF_ID, sizeof(ntp.ref_id));
    ntp.orig_time = ntp.xmit_time;
    ntp.recv_time = recv_time;
    getNTPTime(&(ntp.ref_time));
    dumpNTPPacket(&ntp);
    ntp.delay              = htonl(ntp.delay);
    ntp.dispersion         = htonl(ntp.dispersion);
    ntp.orig_time.seconds  = htonl(ntp.orig_time.seconds);
    ntp.orig_time.fraction = htonl(ntp.orig_time.fraction);
    ntp.ref_time.seconds   = htonl(ntp.ref_time.seconds);
    ntp.ref_time.fraction  = htonl(ntp.ref_time.fraction);
    ntp.recv_time.seconds  = htonl(ntp.recv_time.seconds);
    ntp.recv_time.fraction = htonl(ntp.recv_time.fraction);
    getNTPTime(&(ntp.xmit_time));
    ntp.xmit_time.seconds  = htonl(ntp.xmit_time.seconds);
    ntp.xmit_time.fraction = htonl(ntp.xmit_time.fraction);
    aup.write((uint8_t*)&ntp, sizeof(ntp));
}

void setup()
{
    dbbegin(115200);
    dbprintln("\n\nStartup!");

    pinMode(SYNC_PIN, INPUT);
    pinMode(LED_PIN,  OUTPUT);

    seconds              = 0;
    max_micros           = 0;
    min_micros           = 0;
    last_micros          = 0;
#if defined(MICROS_HISTORY_SIZE)
    micros_history_count = 0;
    micros_history_index = 0;
#endif

    WiFiManager wifi;
    //wifi.setDebugOutput(false);
    String ssid = "SynchroClock" + String(ESP.getChipId());
    wifi.autoConnect(ssid.c_str(), NULL);

    Wire.begin();
    Wire.setClockStretchLimit(1500);
    while (rtc.begin())
    {
        dbprintln("RTC begin failed! Attempting recovery...");

        while (WireUtils.clearBus())
        {
            delay(10000);
            dbprintln("lets try that again...");
        }
        delay(1000);
    }

    attachInterrupt(SYNC_PIN, &oneSecondInterrupt, FALLING);
    dbprintf("delay %d seconds to make sure we have a clean last_micros value\n", WARMUP_SECONDS);
    delay(WARMUP_SECONDS*1000);

    precision = computePrecision();

#if 0
    waitForEdge(SYNC_EDGE_FALLING);
    delay(2); // for some reason we get errors if we read too soon after the falling edge.
    DS3231DateTime dt;
    while (rtc.readTime(dt))
    {
        dbprintln("setup: FAILED to read RTC");
        while (WireUtils.clearBus())
        {
            delay(10000);
            dbprintln("lets try that again...");
        }
        delay(1000);
    }
    seconds = dt.getUnixTime();
#endif

    //
    // initialize UDP handler
    //
    while(!udp.listen(NTP_PORT)) {
        dbprintf("setup: failed to listen on port %d!  Will retry in a bit...\n", NTP_PORT);
        delay(1000);
        dbprintf("setup: retrying!\n");
    }

    udp.onPacket(recievePacket);
}


void loop()
{
    static int last_sync_level;

    //
    // insure seconds is correct after each falling edge
    //
    int sync_level = digitalRead(SYNC_PIN);
    if (sync_level == 0 && sync_level != last_sync_level)
    {
        delay(2); // for some reason I get errors if we read too soon after the falling edge.
        updateSeconds();
    }
    last_sync_level = sync_level;

    static uint32_t last_seconds;
    if (seconds != last_seconds && (seconds % 60) == 0)
    {
#if defined(MICROS_HISTORY_SIZE)
        double mean = 0.0;
        for (int i = 0; i < micros_history_count; ++i)
        {
            mean += us2s(micros_history[i]);
        }
        mean = mean / micros_history_count;

        double stdev = 0.0;
        for (int i = 0; i < micros_history_count; ++i)
        {
            stdev += pow(us2s(micros_history[i]) - mean, 2);
        }
        stdev = sqrt(stdev / micros_history_count);
        dbprintf("mean:%f stdev:%f ", mean, stdev);
#endif
        double disp = us2s(max(abs(1000000-max_micros), abs(1000000-min_micros)));
        dbprintf("min:%f max:%f jitter:%f dispersion:%f\n", us2s(min_micros), us2s(max_micros), us2s(max_micros-min_micros), disp);
        dispersion = (uint32_t)(disp * 65536.0);
    }
    last_seconds = seconds;
}
