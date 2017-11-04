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
#define DEBUG
#include "Logger.h"
#include <lwip/def.h> // htonl() & ntohl()

volatile uint32_t seconds;
volatile uint32_t cycles;
volatile uint32_t last_cpu_cycles;
volatile uint32_t min_cycles;
volatile uint32_t max_cycles;

AsyncUDP udp;
DS3231   rtc;                       // real time clock on i2c interface

#ifdef DEBUG
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
    uint32_t cpu_cycles = ESP.getCycleCount();
    //
    // the first time around we just initialize the last value
    //
    if (last_cpu_cycles == 0)
    {
        last_cpu_cycles = cpu_cycles;
        return;
    }

    cycles              = cpu_cycles - last_cpu_cycles;
    last_cpu_cycles     = cpu_cycles;
    seconds += 1;

    if (min_cycles == 0 || cycles < min_cycles)
    {
        min_cycles = cycles;
    }

    if (cycles > max_cycles)
    {
        max_cycles = cycles;
    }

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
    uint32_t cpu_cycles   = ESP.getCycleCount();
    uint32_t cycles_delta = cpu_cycles - last_cpu_cycles;

    //
    // if cycles_delta is at or bigger than cycles then
    // use the max fraction.
    //
    if (cycles_delta >= cycles)
    {
        time->fraction = 0xffffffff;
        return;
    }

    double   percent      = (double)cycles_delta / (double)cycles;
    //dbprintf("cycles_delta: %lu cycles: %lu percent: %lf\n", cycles_delta, cycles, percent);
    time->fraction      = (uint32_t)(percent * (double)4294967296L);
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
    ntp.precision = -18;
    // TODO: root delay, and root dispersion
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
    dbprintln("");
    dbprintln("Startup!");

    pinMode(SYNC_PIN, INPUT);
    pinMode(LED_PIN,  OUTPUT);

    seconds             = 0;
    cycles              = 0;
    max_cycles          = 0;
    min_cycles          = 0;
    last_cpu_cycles     = 0;

    attachInterrupt(SYNC_PIN, &oneSecondInterrupt, FALLING);
    dbprintf("delay 2 seconds to make sure we have a clean cycle count\n");
    delay(2000);

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

    WiFiManager wifi;
    //wifi.setDebugOutput(false);
    String ssid = "SynchroClock" + String(ESP.getChipId());
    wifi.autoConnect(ssid.c_str(), NULL);

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
        DS3231DateTime dt;
        delay(2); // for some reason we get errors if we read too soon after the falling edge.
        if (rtc.readTime(dt))
        {
            dbprintln("loop: FAILED to read RTC, clearing bus & not checking seconds!");
            WireUtils.clearBus();
        }
        else
        {
            uint32_t old_seconds = seconds;
            uint32_t now = dt.getUnixTime();
            if (now != seconds)
            {
                seconds = now;
                dbprintf("loop: updated seconds from %lu to %lu\n", old_seconds, now);
            }
        }
    }
    last_sync_level = sync_level;

#if 1
    static uint32_t last_seconds;
    if (seconds != last_seconds && (seconds % 300) == 0)
    {
        dbprintf("min_cycles:%lu max_cycles:%lu cycles:%lu\n", min_cycles, max_cycles, cycles);
    }
    last_seconds = seconds;
#endif
}
