/*
 * Config.cpp
 *
 *  Created on: Jun 10, 2018
 *      Author: chris.l
 */

#include "Config.h"
#include "FS.h"
#include "Log.h"
#include "ArduinoJson.h"

static const char* TAG = "Config";
static const char* CONFIG_FILE = "Config.json";


Config::Config() : _syslog_host(), _syslog_port(0)
{
}

Config::~Config()
{
}

bool Config::begin()
{
    dlog.info(TAG, "begin: Mounting SPIFFS");

    if (!SPIFFS.begin())
    {
        dlog.warning(TAG, "begin: Formatting SPIFFS!!!!!");
        if (SPIFFS.format())
        {
            dlog.error(TAG, "begin: SPIFFS format failed!");
            return false;
        }
        dlog.info(TAG, "begin: mounting SPIFFS after format!");
        if (!SPIFFS.begin())
        {
            dlog.error(TAG, "begin: SPIFFS mount failed!!!!");
            return false;
        }
    }

    return true;
}

bool Config::load()
{
    dlog.info(TAG, "load: file: '%s'", CONFIG_FILE);

    if (!SPIFFS.exists(CONFIG_FILE))
    {
        dlog.warning(TAG, "load: config file does not exist!");
        return false;
    }

    File f = SPIFFS.open(CONFIG_FILE, "r");
    if (!f)
    {
        dlog.error(TAG, "load: failed to open config file!");
        return false;
    }

    StaticJsonBuffer<512> buffer;

    dlog.debug(TAG, "load: parsing file contents");
    JsonObject& root = buffer.parseObject(f);
    f.close();

    if (!root.success())
    {
        dlog.error(TAG, "load: fails to parse file!");
        return false;
    }

    strlcpy(_syslog_host, root["syslogHost"]|"", sizeof(_syslog_host));
    _syslog_port = root["syslogPort"] | 0;

    dlog.info(TAG, "load: config loaded!");
    return true;
}

void Config::save()
{
    dlog.info(TAG, "save: file: '%s'", CONFIG_FILE);

    File f = SPIFFS.open(CONFIG_FILE, "w");

    StaticJsonBuffer<512> buffer;
    JsonObject& root = buffer.createObject();

    root["syslogHost"] = _syslog_host;
    root["syslogPort"] = _syslog_port;

    root.printTo(f);
    f.close();

    dlog.info(TAG, "save: config saved");
}

const char* Config::getSyslogHost()
{
    return _syslog_host;
}

void Config::setSyslogHost(const char* host)
{
    strlcpy(_syslog_host, host, sizeof(_syslog_host));
}

uint16_t Config::getSyslogPort()
{
    return _syslog_port;
}

void Config::setSyslogPort(uint16_t port)
{
    _syslog_port = port;
}
