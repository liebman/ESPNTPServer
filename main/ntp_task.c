/*
 * ntp_task.c
 *
 *  Created on: Jan 21, 2018
 *      Author: chris.l
 */

#include "esp_gps_ntp.h"

#include <string.h>
#include <sys/socket.h>
#include <math.h>


static const char *TAG = "NTP";

#define NTP_PORT 123


typedef struct ntp_time
{
    uint32_t seconds;
    uint32_t fraction;
} NTPTime;

typedef struct ntp_packet
{
    uint8_t  flags;
    uint8_t  stratum;
    uint8_t  poll;
    int8_t   precision;
    uint32_t delay;
    uint32_t dispersion;
    uint8_t  ref_id[4];
    NTPTime  ref_time;
    NTPTime  orig_time;
    NTPTime  recv_time;
    NTPTime  xmit_time;
} NTPPacket;

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

#define PRECISION_COUNT        10000

#define LI_NONE         0
#define LI_SIXTY_ONE    1
#define LI_FIFTY_NINE   2
#define LI_NOSYNC       3

#define MODE_RESERVED   0
#define MODE_ACTIVE     1
#define MODE_PASSIVE    2
#define MODE_CLIENT     3
#define MODE_SERVER     4
#define MODE_BROADCAST  5
#define MODE_CONTROL    6
#define MODE_PRIVATE    7

#define NTP_VERSION     4

#define REF_ID          "PPS "  // "GPS " when we have one!

#define setLI(value)    ((value&0x03)<<6)
#define setVERS(value)  ((value&0x07)<<3)
#define setMODE(value)  ((value&0x07))

#define getLI(value)    ((value>>6)&0x03)
#define getVERS(value)  ((value>>3)&0x07)
#define getMODE(value)  (value&0x07)

#define SEVENTY_YEARS   2208988800L
#define toEPOCH(t)      ((uint32_t)t-SEVENTY_YEARS)
#define toNTP(t)        ((uint32_t)t+SEVENTY_YEARS)

static int8_t   precision;
static uint32_t req_count;
static uint32_t rsp_count;

void getNTPTaskStatus(NTPTaskStatus* status)
{
    status->req_count = req_count;
    status->rsp_count = rsp_count;
}

static void getNTPTime(NTPTime *time)
{
    time_t seconds;
    time_t usecs;
    getTime(&seconds, &usecs);
    time->seconds = toNTP(seconds);

    //
    // if micros_delta is at or bigger than one second then
    // use the max fraction.
    //
    if (usecs >= 1000000)
    {
        time->fraction = 0xffffffff;
        return;
    }

    time->fraction = (uint32_t)(us2s(usecs) * (double)4294967296L);
}

int8_t computePrecision()
{
    NTPTime t;
    unsigned long start = micros();
    for (int i = 0; i < PRECISION_COUNT; ++i)
    {
        getNTPTime(&t);
    }
    unsigned long end   = micros();
    double        total = (double)(end - start) / 1000000.0;
    double        time  = total / PRECISION_COUNT;
    double        prec  = log2(time);
    ESP_LOGI(TAG, "computePrecision: total:%f time:%f prec:%f\n", total, time, prec);
    return (int8_t)prec;
}

static void ntp_task()
{
    static NTPPacket ntp;
    static int udp_server = -1;

    ESP_LOGI(TAG, "starting!");

    precision = computePrecision();
    ESP_LOGI(TAG, "precision: %d", precision);
    if ((udp_server=socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        ESP_LOGI(TAG, "could not create socket: %d", errno);
        return;
    }

    int yes = 1;
    if (setsockopt(udp_server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0)
    {
        ESP_LOGI(TAG, "could not set socket option: %d", errno);
        return;
    }

    struct sockaddr_in addr;
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(NTP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(udp_server , (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        ESP_LOGI(TAG, "could not bind socket: %d", errno);
        return;
    }

    while(1)
    {
        int len;
        int slen = sizeof(addr);
        if ((len = recvfrom(udp_server, &ntp, sizeof(ntp), 0, (struct sockaddr *) &addr, (socklen_t *)&slen)) == -1)
        {
            ESP_LOGI(TAG, "could not receive data: %d", errno);
            continue;
        }

        ++req_count;

        NTPTime   recv_time;
        getNTPTime(&recv_time);

        if (len != sizeof(NTPPacket))
        {
            ESP_LOGI(TAG, "ignoring packet with bad length: %d < %d\n", len, sizeof(ntp));
            continue;
        }

        if (!isTimeValid())
        {
            ESP_LOGI(TAG, "GPS time data not valid!");
            continue;
        }

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
        ntp.flags      = setLI(LI_NONE) | setVERS(NTP_VERSION) | setMODE(MODE_SERVER);
        ntp.stratum    = 1;
        ntp.precision  = precision;
        // TODO: compute actual root delay, and root dispersion
        ntp.delay = (uint32_t)(0.000001 * 65536);
        ntp.dispersion = getDispersion();
        strncpy((char*)ntp.ref_id, REF_ID, sizeof(ntp.ref_id));
        ntp.orig_time  = ntp.xmit_time;
        ntp.recv_time  = recv_time;
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

        int sent = sendto(udp_server, &ntp, sizeof(ntp), 0, (struct sockaddr*) &addr, sizeof(addr));
        if(sent < 0)
        {
            ESP_LOGI(TAG, "could not send data: %d", errno);
          continue;
        }
        ++rsp_count;
    }
}

void start_ntp()
{
    esp_log_level_set(TAG, CONFIG_NTP_LOG_LEVEL);

    xTaskCreatePinnedToCore(ntp_task,        "ntp_task",        1024*2, NULL, NTP_TASK_PRI, NULL, 1);
}
