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
//define NTP_PACKET_DEBUG
#include "Logger.h"

Ticker            validityTimer;
uint32_t          valid_count;
bool              valid;
bool              sentence_unknown;
int8_t            precision;
volatile uint32_t dispersion;

volatile time_t   seconds;

volatile uint32_t last_micros;
volatile uint32_t micros_wraps;
volatile uint32_t min_micros;
volatile uint32_t max_micros;

#if defined(MICROS_HISTORY_SIZE)
volatile uint32_t micros_history[MICROS_HISTORY_SIZE];
volatile uint16_t micros_history_count;
volatile uint16_t micros_history_index;
#endif

#if defined(USE_ASYNC_UDP)
AsyncUDP udp;
#else
WiFiUDP udp;
#endif

SoftwareSerial gps(GPS_RX_PIN, GPS_TX_PIN, false, SERIAL_BUFFER_SIZE);
char nmeaBuffer[NMEA_BUFFER_SIZE];
MicroNMEA nmea(nmeaBuffer, NMEA_BUFFER_SIZE);

#if defined(USE_OLED_DISPLAY)
SSD1306Wire  display(0x3c, SDA, SCL);
#endif

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

void validityCheck()
{
    valid = false;
}

void oneSecondInterrupt()
{
    uint32_t cur_micros = micros();

    //
    // restart the validity timer, if it runs out we invalidate our data.
    //
    validityTimer.attach_ms(VALIDITY_CHECK_MS, &validityCheck);

    //
    // increment seconds
    //
    seconds += 1;

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

#if defined(DEBUG) && defined(LED_PIN)
    digitalWrite(LED_PIN, digitalRead(LED_PIN) ? LOW : HIGH);
#endif
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

#if defined(USE_ASYNC_UDP)
void recievePacket(AsyncUDPPacket aup)
#else
void recievePacket()
#endif
{
    static NTPPacket ntp;
    NTPTime recv_time;
    getNTPTime(&recv_time);
#if defined(USE_ASYNC_UDP)
    if (aup.length() != sizeof(NTPPacket))
    {
        dbprintf("recievePacket: ignoring packet with bad length: %d < %d\n", aup.length(), sizeof(NTPPacket));
        return;
    }
#else
    if (udp.available() != sizeof(NTPPacket))
    {
        dbprintf("recievePacket: ignoring packet with bad length: %d < %d\n", udp.available(), sizeof(NTPPacket));
        return;
    }
#endif

    if (!valid)
    {
        dbprintln("recievePacket: GPS data not valid!");
        return;
    }

#if defined(USE_ASYNC_UDP)
    memcpy(&ntp, aup.data(), sizeof(ntp));
#else
    udp.read((unsigned char*)&ntp, sizeof(ntp));

#endif
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
#if defined(USE_ASYNC_UDP)
    aup.write((uint8_t*)&ntp, sizeof(ntp));
#else
    IPAddress address = udp.remoteIP();
    uint16_t  port    = udp.remotePort();
    udp.beginPacket(address, port);
    udp.write((uint8_t*)&ntp, sizeof(ntp));
    udp.flush();
    udp.endPacket();
#endif
}

void badChecksum(MicroNMEA& mn)
{
    const char* s = mn.getSentence();
    dbprintf("badChecksum: (length:%d 1stbyte:%02x) '%s'\n", strlen(s), s[0], s);
}

void unknownSentence(MicroNMEA& mn)
{
    const char* sentence = mn.getSentence();
    if (!strncmp("$PMTK", sentence, 5))
    {
        dbprintf("unknownSentence: %s\n", sentence);
        return;
    }

    sentence_unknown = true;
    dbprintf("unknownSentence: %s\n", sentence);
}

void resetGPS()
{
    dbprintln("resetGPS: starting!");
    // Empty input buffer
    while (gps.available())
    gps.read();

    digitalWrite(GPS_EN_PIN, LOW);
    delay(100);
    digitalWrite(GPS_EN_PIN, HIGH);

    dbprintln("resetGPS: waiting on first sentence");
    dbflush();

    // Reset is complete when the first valid message is received
    while (1)
    {
        delay(1);
        while (gps.available())
        {
            char c = gps.read();
            if (nmea.process(c))
            {
                const char* sentence = nmea.getSentence();
                dbprintf("resetGPS: done, sentence: '%s'\n", sentence);
                dbflush();
                return;
            }
        }
    }
}

void processGPS()
{
    static boolean last_valid;
    if (last_valid && !valid)
    {
        dbprintln("INVALID!");
    }
    last_valid = valid;

    while(gps.available() > 0)
    {
        if (nmea.process(gps.read()))
        {
            if (nmea.isValid() && nmea.getYear() > 2000)
            {
                static struct tm tm;
                tm.tm_year = nmea.getYear()  - 1900;
                tm.tm_mon  = nmea.getMonth() - 1;
                tm.tm_mday = nmea.getDay();
                tm.tm_hour = nmea.getHour();
                tm.tm_min  = nmea.getMinute();
                tm.tm_sec  = nmea.getSecond();
                time_t new_seconds = mktime(&tm);

                time_t old_seconds = seconds;
                if (old_seconds != new_seconds)
                {
                    seconds = new_seconds;
                    dbprintf("adjusting seconds from %lu to %lu\n", old_seconds, new_seconds);
                }

                //
                // if we were not valid, we are now
                //
                if (!valid)
                {
                    // clear stats and mark us valid
                    last_micros = 0;
                    min_micros  = 0;
                    max_micros  = 0;
                    valid = true;
                    ++valid_count;
                    dbprintln("VALID!");
                }
            }
        }
    }
}

void sendSentence(const char* sentence)
{
    static char cksum[3];
    MicroNMEA::generateChecksum(sentence, cksum);
    cksum[2] = '\0';
    dbprintf("sendSentence: '%s*%s'\n", sentence, cksum);
    gps.printf("%s*%s\r\n", sentence, cksum);
    gps.flush();
}

void setup()
{
    dbbegin(115200);
    dbprintln("\n\nStartup!");

    pinMode(SYNC_PIN, INPUT);
#if defined(LED_PIN)
    pinMode(LED_PIN,  OUTPUT);
#endif

    valid                = false;
    valid_count          = 0;
    seconds              = 0;
    max_micros           = 0;
    min_micros           = 0;
    last_micros          = 0;
#if defined(MICROS_HISTORY_SIZE)
    micros_history_count = 0;
    micros_history_index = 0;
#endif

#if !defined(USE_NO_WIFI)
    WiFiManager wifi;
    //wifi.setDebugOutput(false);
    String ssid = "SynchroClock" + String(ESP.getChipId());
    wifi.autoConnect(ssid.c_str(), NULL);
#endif

#if defined(USE_OLED_DISPLAY)
    if (!display.init())
    {
        dbprintln("display.init() failed!");
    }
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
#endif

    gps.begin(9600);
    nmea.setBadChecksumHandler(&badChecksum);
    nmea.setUnknownSentenceHandler(&unknownSentence);
    resetGPS();

    precision = computePrecision();

    //
    // initialize UDP handler
    //
#if defined(USE_ASYNC_UDP)
    while(!udp.listen(NTP_PORT)) {
#else
    while(!udp.begin(NTP_PORT)) {
#endif
        dbprintf("setup: failed to listen on port %d!  Will retry in a bit...\n", NTP_PORT);
        delay(1000);
        dbprintf("setup: retrying!\n");
    }

    attachInterrupt(SYNC_PIN, &oneSecondInterrupt, FALLING);

#if defined(USE_ASYNC_UDP)
    udp.onPacket(recievePacket);
#endif
}

void loop()
{
#if !defined(USE_ASYNC_UDP)
    if (udp.parsePacket())
    {
        recievePacket();
    }
#endif

    processGPS();
    if (sentence_unknown)
    {
        sentence_unknown = false;
        // Send only RMC and GGA messages.
        sendSentence("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
    }

    static time_t last_seconds;
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
        double disp = us2s(MAX(abs(MICROS_PER_SEC-max_micros), abs(MICROS_PER_SEC-min_micros)));
        dispersion = (uint32_t)(disp * 65536.0);
        dbprintf("min:%lu max:%lu jitter:%lu valid_count:%lu valid:%s\n",
                min_micros, max_micros, max_micros-min_micros,
                valid_count, valid?"true":"false");
    }
    last_seconds = seconds;

    //
    // Update the display
    //
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    const char* s = ctime(&last_seconds);
    display.drawString(0, 0,  WiFi.localIP().toString());
    display.drawString(0, 10, s);
    // write the buffer to the display
    display.display();

    delay(1);
}
