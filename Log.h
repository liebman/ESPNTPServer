/*
 * Log.h
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
 *  Created on: Mar 2, 2018
 *      Author: chris.l
 */

#ifndef LOG_H_
#define LOG_H_

#include "Print.h"
#include <string>
#include <map>
#include <stdarg.h>

typedef enum log_level
{
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_MAX,
} LogLevel;

#define LOG_MAX_SIZE 80
#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO

class Log
{
public:
    static Log& getLog();

    void setSize(size_t size);
    void setLevel(LogLevel level);
    void setLevel(const char* name, LogLevel level);
    void setPrint(Print *print);
    void setPreFunc(std::function<void(Print* print)> func);

    void error(const char* name, const char* fmt, ...);
    void warning(const char* name, const char* fmt, ...);
    void info(const char* name, const char* fmt, ...);
    void debug(const char* name, const char* fmt, ...);
    void trace(const char* name, const char* fmt, ...);

    // we don't allow copying this guy!
    Log(const Log&)            = delete;
    Log& operator=(const Log&) = delete;

private:
    LogLevel                        _level;      // default log level
    size_t                          _size;
    char*                           _buffer;
    std::map<std::string, LogLevel> _levels;
    Print*                          _print;

    std::function<void(Print* print)> pre_func;


    Log();
    virtual ~Log();
    void print(const char* name, LogLevel level, const char*fmt, va_list ap);
};

extern Log& logger;

#endif /* LOG_H_ */
