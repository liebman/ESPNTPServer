/*
 * Display.cpp
 *
 *  Created on: Mar 4, 2018
 *      Author: chris.l
 */

#include <stdarg.h>
#include <time.h>
#include "ESP8266WiFi.h"
#include "Display.h"
#include "Log.h"
#include "DialogInput_plain_10.h"

static const char* TAG = "Display";

Display::Display(GPS& gps, NTP& ntp, uint8_t sda, uint8_t scl) : _dsp(0x3c, sda, scl), _gps(gps), _ntp(ntp)
{
    // TODO Auto-generated constructor stub

}

Display::~Display()
{
    // TODO Auto-generated destructor stub
}

void Display::begin()
{
    dlog.info(TAG, F("initializing display"));
    if (!_dsp.init())
    {
        dlog.error(TAG, F("display.init() failed!"));
    }
    _dsp.flipScreenVertically();
    font(ArialMT_Plain_10);
}

void Display::end()
{
    _dsp.end();
}

void Display::process()
{
    time_t secs = _gps.getSeconds();;
    struct tm tm;
    gmtime_r(&secs, &tm);

    clear();
    font(DialogInput_plain_10);
    IPAddress ip = WiFi.localIP();

    const char* wifi_status_str;
    int wifi_status = WiFi.status();
    switch (wifi_status)
    {
        case WL_CONNECTED:
            wifi_status_str = "READY";
            break;
        case WL_NO_SSID_AVAIL:
            wifi_status_str = "NSSID";
            break;
        case WL_CONNECT_FAILED:
            wifi_status_str = "FAIL";
            break;
        case WL_IDLE_STATUS:
            wifi_status_str = "IDLE";
            break;
        case WL_DISCONNECTED:
            wifi_status_str = "DISCON";
            break;
        default:
            wifi_status_str = "<UNK>";
            break;
    }

    align(TEXT_ALIGN_CENTER);
    print(64, 0, "%04d/%02d/%02d %02d:%02d:%02d",
          tm.tm_year+1900,
          tm.tm_mon+1,
          tm.tm_mday,
          tm.tm_hour,
          tm.tm_min,
          tm.tm_sec);

    align(TEXT_ALIGN_LEFT);
    print(0, 10, "Sats: %d", _gps.getSatelliteCount());
    print(0, 20, "Reqs: %d", _ntp.getReqCount());
    print(0, 30, "Rsps: %d", _ntp.getRspCount());

    if (_gps.isValid())
    {
        uint32_t seconds  = secs - _gps.getValidSince();
        uint32_t days     = seconds / 86400;
        seconds          -= days * 86400;
        uint32_t hours    = seconds /3600;
        seconds          -= hours * 3600;
        uint32_t minutes  = seconds / 60;
        seconds          -= minutes * 60;
        align(TEXT_ALIGN_RIGHT);
        print(127, 40, "%dd %02dh %02dm %02ds", days, hours, minutes, seconds);
    }
    else
    {
        if (_gps.isGPSValid())
        {
            align(TEXT_ALIGN_RIGHT);
            print(127, 40, "%d till VALID", _gps.getValidDelay());
        }
        else
        {
            align(TEXT_ALIGN_CENTER);
            print(64, 40, "GPS NOT VALID");
        }
    }

    align(TEXT_ALIGN_RIGHT);
    print(127, 50, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    align(TEXT_ALIGN_LEFT);
    print(0,   50, "%s", wifi_status_str);
    display();
}

void Display::clear()
{
    _dsp.clear();
}

void Display::display()
{
    _dsp.display();
}

void Display::font(const char* fontData)
{
    _dsp.setFont(fontData);
}

void Display::align(OLEDDISPLAY_TEXT_ALIGNMENT alignment)
{
    _dsp.setTextAlignment(alignment);
}

void Display::print(int16_t x, int16_t y, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(_buffer, DISPLAY_BUFFER_LEN, fmt, ap);
    _buffer[DISPLAY_BUFFER_LEN-1] = '\0';
    va_end(ap);

    _dsp.drawString(x, y, _buffer);
}
