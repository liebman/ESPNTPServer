/*
 * esp_gps_ntp.h
 *
 *  Created on: Jan 21, 2018
 *      Author: chris.l
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#ifndef MAIN_ESP_GPS_NTP_H_
#define MAIN_ESP_GPS_NTP_H_

#define MICROS_PER_SEC         1000000
#define us2s(x) (((double)x)/(double)MICROS_PER_SEC) // microseconds to seconds

#define SDA_PIN (GPIO_NUM_21)
#define SCL_PIN (GPIO_NUM_22)

typedef uint64_t micros_t;
typedef struct gps_task_status
{
    uint32_t      timeouts;
    time_t        seconds;
    micros_t      min_micros;
    micros_t      max_micros;
    bool          gps_valid;
    bool          valid;
    uint32_t      valid_in;
    time_t        valid_since;
    uint32_t      valid_count;
    time_t        initial_valid;
    int           sat_count;
    int           fix_quality;
} GPSTaskStatus;

typedef struct ntp_task_status
{
    uint32_t req_count;
    uint32_t rsp_count;
} NTPTaskStatus;

#define GPS_TASK_PRI    configMAX_PRIORITIES
#define NTP_TASK_PRI    (configMAX_PRIORITIES-1)
#define DSP_TASK_PRI    (configMAX_PRIORITIES-2)

extern void start_gps();
extern void start_ntp();
extern void start_display();

extern void triggerDisplay();
extern void getTime(time_t *secs, time_t *usecs);
extern void getGPSTaskStatus(GPSTaskStatus* status);
extern void getNTPTaskStatus(NTPTaskStatus* status);
extern bool isTimeValid();
extern micros_t micros();
extern uint32_t getDispersion();

#endif /* MAIN_ESP_GPS_NTP_H_ */
