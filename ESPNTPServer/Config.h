/*
 * Config.h
 *
 *  Created on: Jun 10, 2018
 *      Author: chris.l
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "Arduino.h"

class ConfigImpl;

class Config
{
public:
    Config();
    virtual ~Config();

    bool        begin();

    bool        load();
    void        save();

    const char* getSyslogHost();
    void        setSyslogHost(const char* host);
    uint16_t    getSyslogPort();
    void        setSyslogPort(uint16_t port);

private:
    char     _syslog_host[64];
    uint16_t _syslog_port;
};

#endif /* CONFIG_H_ */
