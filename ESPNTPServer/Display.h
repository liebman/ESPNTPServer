/*
 * Display.h
 *
 *  Created on: Mar 4, 2018
 *      Author: chris.l
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "SSD1306Wire.h"
#include "GPS.h"
#include "NTP.h"

#define DISPLAY_BUFFER_LEN  32

class Display
{
public:
    Display(GPS& gps, NTP& ntp, uint8_t sda, uint8_t scl);
    virtual ~Display();

    void begin();
    void end();
    void process();

    void message(const char* fmt, ...);

    void clear();
    void display();
    void align(OLEDDISPLAY_TEXT_ALIGNMENT alignment);
    void font(const char* fontData);

    void print(int16_t x, int16_t y, const char* fmt, ...);
    void vprint(int16_t x, int16_t y, const char* fmt, va_list ap);
private:
    SSD1306Wire    _dsp;
    GPS&           _gps;
    NTP&           _ntp;
    char           _buffer[DISPLAY_BUFFER_LEN];
};

#endif /* DISPLAY_H_ */
