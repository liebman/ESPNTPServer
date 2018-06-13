/*
 * NTP.cpp
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
 *  Created on: Feb 27, 2018
 *      Author: chris.l
 */

#include <functional>
#include <lwip/def.h> // htonl() & ntohl()

#include "NTP.h"

#include "Log.h"
static const char* TAG = "NTP";

//#define NTP_PACKET_DEBUG

#define NTP_PORT               123
#define PRECISION_COUNT        10000

typedef struct ntp_packet
{
    uint8_t  flags;
    uint8_t  stratum;
    uint8_t  poll;
    int8_t   precision;
    uint32_t delay;
    uint32_t dispersion;
    uint8_t  ref_id[4];
    NTPTime  ref_time;
    NTPTime  orig_time;
    NTPTime  recv_time;
    NTPTime  xmit_time;
} NTPPacket;

#define LI_NONE         0
#define LI_SIXTY_ONE    1
#define LI_FIFTY_NINE   2
#define LI_NOSYNC       3

#define MODE_RESERVED   0
#define MODE_ACTIVE     1
#define MODE_PASSIVE    2
#define MODE_CLIENT     3
#define MODE_SERVER     4
#define MODE_BROADCAST  5
#define MODE_CONTROL    6
#define MODE_PRIVATE    7

#define NTP_VERSION     4

#define REF_ID          "PPS "  // "GPS " when we have one!

#define setLI(value)    ((value&0x03)<<6)
#define setVERS(value)  ((value&0x07)<<3)
#define setMODE(value)  ((value&0x07))

#define getLI(value)    ((value>>6)&0x03)
#define getVERS(value)  ((value>>3)&0x07)
#define getMODE(value)  (value&0x07)

#define SEVENTY_YEARS   2208988800L
#define toEPOCH(t)      ((uint32_t)t-SEVENTY_YEARS)
#define toNTP(t)        ((uint32_t)t+SEVENTY_YEARS)

#ifdef NTP_PACKET_DEBUG
#include <time.h>
char* timestr(long int t)
{
    t = toEPOCH(t);
    return ctime(&t);
}

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

NTP::NTP(GPS& gps) :
    _gps(gps),
    _udp(),
    _req_count(0),
    _rsp_count(0),
    _precision(0)
{
}

NTP::~NTP()
{
}

void NTP::begin()
{
    _precision = computePrecision();
    while (!_udp.listen(NTP_PORT))
    {
        dlog.error(TAG, F("failed to listen on port %d!  Will retry in a bit..."), NTP_PORT);
        delay(1000);
        dlog.warning(TAG, F("setup: retrying!"));
    }

    using namespace std::placeholders;  // for _1, _2, _3...

    _udp.onPacket(std::bind( &NTP::ntp, this, _1));
}

int8_t NTP::computePrecision()
{
    NTPTime t;
    unsigned long start = micros();
    for (int i = 0; i < PRECISION_COUNT; ++i)
    {
        getNTPTime(&t);
    }
    unsigned long end   = micros();
    double        total = (double)(end - start) / 1000000.0;
    double        time  = total / PRECISION_COUNT;
    double        prec  = log2(time);
    dlog.info(TAG, F("computePrecision: total:%f time:%f prec:%f (%d)"), total, time, prec, (int8_t)prec);
    return (int8_t)prec;
}

void NTP::getNTPTime(NTPTime *time)
{
    struct timeval tv;
    _gps.getTime(&tv);
    time->seconds = toNTP(tv.tv_sec);

    double percent = us2s(tv.tv_usec);
    time->fraction = (uint32_t)(percent * (double)4294967296L);
}

void NTP::ntp(AsyncUDPPacket& aup)
{
    ++_req_count;
    NTPPacket ntp;
    NTPTime   recv_time;
    getNTPTime(&recv_time);
    if (aup.length() != sizeof(NTPPacket))
    {
        dlog.warning(TAG, F("recievePacket: ignoring packet with bad length: %d < %d"), aup.length(), sizeof(NTPPacket));
        return;
    }

    if (!_gps.isValid())
    {
        dlog.warning(TAG, F("recievePacket: GPS data not valid!"));
        return;
    }

    memcpy(&ntp, aup.data(), sizeof(ntp));
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
    ntp.flags      = setLI(LI_NONE) | setVERS(NTP_VERSION) | setMODE(MODE_SERVER);
    ntp.stratum    = 1;
    ntp.precision  = _precision;
    // TODO: compute actual root delay, and root dispersion
    ntp.delay = 1;      //(uint32)(0.000001 * 65536.0);
    ntp.dispersion = 1; //(uint32_t)(_gps.getDispersion() * 65536.0); // TODO: pre-calculate this?
    strncpy((char*)ntp.ref_id, REF_ID, sizeof(ntp.ref_id));
    ntp.orig_time  = ntp.xmit_time;
    ntp.recv_time  = recv_time;
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
    ++_rsp_count;
}
