/*
 * Log.cpp
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
#include "Arduino.h"
#include "Log.h"

static const char* level_names[LOG_LEVEL_MAX] = {
        "NONE",
        "ERROR",
        "WARN",
        "INFO",
        "DEBUG",
        "TRACE"
};

Log& Log::getLog()
{
    static Log log;
    return log;
}

Log::Log() :
        _level(LOG_LEVEL_INFO),
        _size(LOG_MAX_SIZE),
        _buffer(nullptr),
        _print(nullptr)
{
}

Log::~Log()
{
    if (_buffer != nullptr)
    {
        delete[] _buffer;
    }
}

void Log::setSize(size_t size)
{
    _size = size;
    if (_buffer)
    {
        delete[] _buffer;
        _buffer = nullptr;
    }
}

void Log::setLevel(LogLevel level)
{
    _level = level;
}

void Log::setLevel(const char* name, LogLevel level)
{
    _levels[name] = level;
}

void Log::setPrint(Print *print)
{
    _print = print;
}
void Log::setPreFunc(std::function<void(Print* print)> func)
{
    pre_func = func;
}

void Log::error(const char* name, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(name, LOG_LEVEL_ERROR, fmt, ap);
    va_end(ap);
}

void Log::warning(const char* name, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(name, LOG_LEVEL_WARNING, fmt, ap);
    va_end(ap);
}

void Log::info(const char* name, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(name, LOG_LEVEL_INFO, fmt, ap);
    va_end(ap);
}

void Log::debug(const char* name, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(name, LOG_LEVEL_DEBUG, fmt, ap);
    va_end(ap);
}

void Log::trace(const char* name, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    print(name, LOG_LEVEL_TRACE, fmt, ap);
    va_end(ap);
}

void Log::print(const char* name, LogLevel level, const char* fmt, va_list ap)
{
    LogLevel limit = _level;
    if (_levels.find(name) != _levels.end())
    {
        limit = _levels[name];
    }

    if (level > limit)
    {
        return;
    }

    //
    // allocate the buffer on first use
    //
    if (_buffer == nullptr)
    {
        _buffer = new char[_size];
    }

    if (_print == nullptr)
    {
        _print = &Serial;
    }

    vsnprintf(_buffer, _size-1, fmt, ap);

    if (pre_func)
    {
        pre_func(_print);
    }

    _print->printf("%s %5s %s\n", name, level_names[level], _buffer);
}

Log& logger = Log::getLog();
