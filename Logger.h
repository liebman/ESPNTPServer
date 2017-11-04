/*
 * Logger.h
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
 *  Created on: May 31, 2017
 *      Author: liebman
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <Arduino.h>
#include <ESP8266WiFi.h>

#define USE_TCP
#define USE_NETWORK

#ifdef USE_NETWORK
#ifdef USE_TCP
#include <WiFiClient.h>
#else
#include <WiFiUDP.h>
#endif
#endif

#define LOGGER_DEFAULT_BAUD 115200L
#define LOGGER_BUFFER_SIZE  256

class Logger {
public:
	Logger();
	void begin();
	void begin(long int baud);
	void end();
	void setNetworkLogger(const char* host, uint16_t port);
	void println(const char*message);
	void printf(const char*message, ...);
	void flush();

private:
#ifdef USE_NETWORK
#ifdef USE_TCP
	WiFiClient _client;
#else
    WiFiUDP     _udp;
#endif
#endif
    const char* _host;
    uint16_t    _port;
    uint16_t    _failed;
    char        _buffer[LOGGER_BUFFER_SIZE];

    void send(const char* message);
};

extern Logger logger;

#ifdef DEBUG
#define dbbegin(x)      logger.begin(x)
#define dbnetlog(h, p)  {if (strlen(h) && p) logger.setNetworkLogger(h, p);}
#define dbend()         logger.end()
#define dbprintf(...)   logger.printf(__VA_ARGS__)
#define dbprintln(x)    logger.println(x)
#define dbprint64(l,v)  logger.printf("%s %08x:%08x (%Lf)\n", l, (uint32_t)(v>>32), (uint32_t)(v & 0xffffffff), ((long double)v / 4294967296.))
#define dbprint64s(l,v) logger.printf("%s %08x:%08x (%Lf)\n", l,  (int32_t)(v>>32), (uint32_t)(v & 0xffffffff), ((long double)v / 4294967296.))
#define dbflush()       logger.flush()
#else
#define dbbegin(x)
#define dbnetlog(h, p)
#define dbend()
#define dbprintf(...)
#define dbprintln(x)
#define dbprint64(l,v)
#define dbprint64s(l,v)
#define dbflush()
#endif
#endif /* LOGGER_H_ */
