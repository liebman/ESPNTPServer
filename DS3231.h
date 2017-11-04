/*
 * DS3231.h
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
 *  Created on: May 21, 2017
 *      Author: chris.l
 */

#ifndef DS3231_H_
#define DS3231_H_
#include <Arduino.h>
#include <Wire.h>
#include "WireUtils.h"
#include "DS3231DateTime.h"

// I2C address
const uint8_t DS3231_ADDRESS       = 0x68;

// Registers
const uint8_t DS3231_SEC_REG       = 0x00;
const uint8_t DS3231_MIN_REG       = 0x01;
const uint8_t DS3231_HOUR_REG      = 0x02;
const uint8_t DS3231_WDAY_REG      = 0x03;
const uint8_t DS3231_MDAY_REG      = 0x04;
const uint8_t DS3231_MONTH_REG     = 0x05;
const uint8_t DS3231_YEAR_REG      = 0x06;

const uint8_t DS3231_A1S_REG       = 0x07;
const uint8_t DS3231_A1M_REG       = 0x08;
const uint8_t DS3231_A1H_REG       = 0x09;
const uint8_t DS3231_A1W_REG       = 0x0A;

const uint8_t DS3231_AL2M_REG      = 0x0B;
const uint8_t DS3231_AL2H_REG      = 0x0C;
const uint8_t DS3231_AL2W_REG      = 0x0D;

const uint8_t DS3231_CONTROL_REG   = 0x0E;
const uint8_t DS3231_STATUS_REG    = 0x0F;
const uint8_t DS3231_AGING_REG     = 0x0F;
const uint8_t DS3231_TEMP_UP_REG   = 0x11;
const uint8_t DS3231_TEMP_LOW_REG  = 0x12;

// DS3231 Control Register Bits
const uint8_t DS3231_CTL_A1IE      = 0;
const uint8_t DS3231_CTL_A2IE      = 1;
const uint8_t DS3231_CTL_INTCN     = 2;
const uint8_t DS3231_CTL_RS1       = 3;
const uint8_t DS3231_CTL_RS2       = 4;
const uint8_t DS3231_CTL_CONV      = 5;
const uint8_t DS3231_CTL_BBSQW     = 6;
const uint8_t DS3231_CTL_EOSC      = 7;

// DS3231 Status Register Bits
const uint8_t DS3231_STS_A1F       = 0;
const uint8_t DS3231_STS_A2F       = 1;
const uint8_t DS3231_STS_BSY       = 2;
const uint8_t DS3231_STS_EN32KHZ   = 3;
const uint8_t DS3231_STS_OSF       = 7;

// DS3231 Hour register
const uint8_t DS3231_AMPM          = 6;    // AMPM bit
const uint8_t DS3231_24HR_MASK     = 0x3f; // mask for 24 hour value

// DS3231 Month register
const uint8_t DS3231_CENTURY       = 7;
const uint8_t DS3231_MONTH_MASK    = 0x1f;

#define RTC_POSITION_ERROR 0xffff


class DS3231
{
public:
    DS3231();
    int	 begin();
    int      readTime(DS3231DateTime& dt);  // return 0 if ok
    int      writeTime(DS3231DateTime& dt); // return 0 if ok

private:
    uint8_t fromBCD(uint8_t val);
    uint8_t toBCD(uint8_t   val);
    int     setupRead(uint8_t reg, uint8_t size);
    int     write(uint8_t reg, uint8_t value);
    int     read(uint8_t  reg, uint8_t* value);
};

#endif /* DS3231_H_ */
