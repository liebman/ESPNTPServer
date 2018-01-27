/*
 * gps_task.c
 *
 *  Created on: Jan 20, 2018
 *      Author: chris.l
 */

#include "esp_gps_ntp.h"

#include "driver/uart.h"
#include "freertos/timers.h"
#include "sys/time.h"
#include "minmea.h"

static const char *TAG = "GPS";

#define RX_BUF_SIZE 255

#define ESP_INTR_FLAG_DEFAULT 0

//  simple versions - we don't worry about side effects
#define MAX(a, b)   ((a) < (b) ? (b) : (a))
#define MIN(a, b)   ((a) < (b) ? (a) : (b))

#define VALIDITY_TIMER_MS      1010
#define PPS_VALID_COUNT        10   // must have at least this many "good" PPS interrupts to be valid

#define PPS_PIN  (GPIO_NUM_19)
#define TXD_PIN  (GPIO_NUM_17)
#define RXD_PIN  (GPIO_NUM_16)
#define LED_PIN  (GPIO_NUM_15)

#define GPS_UART (UART_NUM_2)

static          TimerHandle_t timer;
static volatile uint32_t      timeouts;
static volatile time_t        seconds;
static volatile micros_t      last_micros;
static volatile micros_t      min_micros;
static volatile micros_t      max_micros;
static volatile bool          gps_valid;
static volatile bool          valid;
static volatile uint32_t      valid_in;
static volatile time_t        valid_since;
static volatile uint32_t      valid_count;
static volatile time_t        initial_valid;
static          uint32_t      dispersion;
static volatile char          reason[128];

typedef struct gps_status
{
    bool   valid;
    int    sat_count;
    int    fix_quality;
} GPSStatus;

static          GPSStatus     gps_status;



#ifdef LED_PIN
#define DEBUG_LED_ON()   gpio_set_level(LED_PIN, 1)
#define DEBUG_LED_OFF()  gpio_set_level(LED_PIN, 0)
#define DEBUG_LED_INIT() init_debug_led()
static void init_debug_led()
{
    //
    // set up the LED pin
    //

    gpio_config_t io_conf;
    //interrupt disabled
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as input mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL<<LED_PIN);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    DEBUG_LED_OFF();
}
#else
#define DEBUG_LED_ON()
#define DEBUG_LED_OFF()
#define DEBUG_LED_INIT()
#endif


uint32_t getDispersion()
{
    return dispersion;
}

bool isTimeValid()
{
    return valid;
}

void getTime(time_t *secs, time_t *usecs)
{
    *secs = seconds;
    *usecs = micros() - last_micros;
}

void getGPSTaskStatus(GPSTaskStatus* status)
{
    status->timeouts      = timeouts;
    status->seconds       = seconds;
    status->min_micros    = min_micros;
    status->max_micros    = max_micros;
    status->gps_valid     = gps_valid;
    status->valid         = valid;
    status->valid_in      = valid_in;
    status->valid_since   = valid_since;
    status->valid_count   = valid_count;
    status->initial_valid = initial_valid;
    status->sat_count     = gps_status.sat_count;
    status->fix_quality   = gps_status.fix_quality;
}

static void IRAM_ATTR pps_isr_handler(void* arg)
{
    DEBUG_LED_ON();
    micros_t cur_micros = micros();
    // restart the validity timer
    xTimerReset(timer, 0);

    //
    // trigger a display update each second
    //
    triggerDisplay();

    //
    // don't trust PPS if GPS is not valid.
    //
    if (!gps_valid)
    {
        DEBUG_LED_OFF();
        return;
    }

    //
    // if we are still counting down then keep waiting
    //
    if (valid_in)
    {
        --valid_in;
        if (valid_in == 0)
        {
            // clear stats and mark us valid
            min_micros  = 0;
            max_micros  = 0;
            valid       = true;
            valid_since = seconds;
            ++valid_count;
            reason[0] = '\0';
            if (initial_valid == 0)
            {
                initial_valid = seconds;
            }
        }
    }

    seconds++;
    //
    // the first time around we just initialize the last value
    //
    if (last_micros == 0)
    {
        last_micros = cur_micros;
        DEBUG_LED_OFF();
        return;
    }

    uint32_t micros_count = cur_micros - last_micros;
    last_micros           = cur_micros;

    if (min_micros == 0 || micros_count < min_micros)
    {
        min_micros = micros_count;
    }

    if (micros_count > max_micros)
    {
        max_micros = micros_count;
    }

    DEBUG_LED_OFF();
}

//
// read a single NMEA record
//
static char *readNMEA()
{
    static uint8_t line[RX_BUF_SIZE+1];
    int size;
    uint8_t *p = line;
    *p = '\0';
    while(1)
    {
        if (p >= line+RX_BUF_SIZE)
        {
            ESP_LOGI(TAG, "Read buffer overflow! size %d bytes: '%s'", p-line, line);
        }
        size = uart_read_bytes(GPS_UART, (unsigned char *)p, 1, portMAX_DELAY);
        if (size == 1)
        {
            if (*p == '$')
            {
                //
                // '$' starts a record so reset pointer to start
                //
                p = line;
                *p = '$';
            }
            else if (*p == '\r' || line[0] != '$')
            {
                //
                // skip newlin and any chars if buffer does not start with '$'
                continue;
            }
            else if (*p == '\n')
            {
                //
                // got a record!
                //
                *p = 0;
                break;
            }
            p++;
        }
    }

    return (char*)line;
}



static void IRAM_ATTR invalidate(const char* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    if (reason[0] == '\0')
    {
        vsnprintf((char*)reason, sizeof(reason)-1, fmt, argp);
    }
    va_end(argp);

    if (valid)
    {
        valid_since = seconds;
    }

    valid       = false;
    gps_valid   = false;
    last_micros = 0;
    valid_in    = 0;
}

static void init_pps()
{
    //
    // set up the PPS interrupt
    //

    gpio_config_t io_conf;
    //interrupt of falling edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL<<PPS_PIN);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(PPS_PIN, pps_isr_handler, (void*)0);
}

static void init_uart()
{
    //
    // initialize the uart attached to the GPS module
    //
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(GPS_UART, &uart_config);
    uart_set_pin(GPS_UART, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // We won't use a buffer for sending data.
    uart_driver_install(GPS_UART, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

static void IRAM_ATTR invalidateTime()
{
    // only count timeouts after we have been valid once.
    if (initial_valid != 0)
    {
        ++timeouts;
    }
    xTimerStart(timer, 0);
    invalidate("TIMEOUT");
    //
    // trigger a display update each timeout
    //
    triggerDisplay();
}

static void init_timer()
{
    timer = xTimerCreate("MyTimer", pdMS_TO_TICKS(VALIDITY_TIMER_MS), pdFALSE, NULL, &invalidateTime);
    xTimerStart(timer, 0);
}

static void gps_task()
{
    static bool last_valid     = false;
    static bool last_gps_valid = false;
    static int  timewarps      = 0;

    DEBUG_LED_INIT();
    init_uart();

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    init_pps();
    init_timer();

    while (1) {

        //
        // Print valid or invalid if status has changed.
        //
        if ((last_valid && !valid) || (last_gps_valid && !gps_valid))
        {
            ESP_LOGW(TAG, "INVALID: '%s'", reason);
            reason[0] = '\0';
        }
        else if (!last_valid && valid)
        {
            ESP_LOGI(TAG, "VALID!");
        }
        last_valid = valid;
        last_gps_valid = gps_valid;

        char* line = readNMEA();

        switch (minmea_sentence_id(line, true))
        {
            case MINMEA_SENTENCE_RMC:
            {
                struct minmea_sentence_rmc frame;
                if (minmea_parse_rmc(&frame, line))
                {
                    ESP_LOGD(TAG, "$RMC: seconds: %lu timeouts: %u valid: %s", seconds, timeouts, frame.valid ? "true" : "false");
                    gps_status.valid = frame.valid;

                    if (frame.valid)
                    {
                        struct timespec ts;
                        minmea_gettime(&ts, &frame.date, &frame.time);
                        if (valid && seconds > ts.tv_sec)
                        {
                            timewarps += 1;
                            ESP_LOGD(TAG, "$RMC: ignoring timewarp back! delayed serial? %lu -> %lu", seconds, ts.tv_sec);
                            if (timewarps > 1)
                            {
                                invalidate("time warped backwards too many (%d) times!", timewarps);
                                timewarps = 0;
                            }
                        }
                        else if (seconds != ts.tv_sec)
                        {
                            ESP_LOGW(TAG, "$RMC: adjusting seconds %lu -> %lu", seconds, ts.tv_sec);
                            seconds = ts.tv_sec;
                        }
                        else
                        {
                            timewarps = 0;
                        }

                        //
                        // if gps was not valid, it is now
                        //
                        if (!gps_valid)
                        {
                            gps_valid       = true;
                            valid_in        = PPS_VALID_COUNT;
                            ESP_LOGI(TAG, "gps valid!");
                        }
                    }
                    else
                    {
                        if (valid)
                        {
                            invalidate("GPS $RMC");
                        }
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "$RMC: failed to parse line: %s", line);
                }
            }
            break;

            case MINMEA_SENTENCE_GGA:
            {
                struct minmea_sentence_gga frame;
                if (minmea_parse_gga(&frame, line)) {
                    ESP_LOGD(TAG, "$GGA: seconds: %lu timeouts: %u fix: %d sats: %d", seconds, timeouts, frame.fix_quality, frame.satellites_tracked);
                    gps_status.sat_count   = frame.satellites_tracked;
                    gps_status.fix_quality = frame.fix_quality;
                }
                else
                {
                    ESP_LOGE(TAG, "$GGA: failed to parse line: %s", line);
                }
            }
            break;

            case MINMEA_SENTENCE_GSV:
            case MINMEA_SENTENCE_GSA:
            case MINMEA_SENTENCE_GLL:
            case MINMEA_SENTENCE_GST:
            case MINMEA_SENTENCE_VTG:
            case MINMEA_SENTENCE_ZDA:
                ESP_LOGV(TAG, "IGNORING: %s", line);
                break;

            case MINMEA_INVALID:
                ESP_LOGE(TAG, "INVALID: '%s'", line);
                break;

            case MINMEA_UNKNOWN:
                ESP_LOGW(TAG, "UNKNOWN: '%s'", line);
                break;
        }

        //
        // Recompute dispersion periodically
        //
        static time_t last_seconds;
        if (seconds != last_seconds)
        {
            double disp = us2s(MAX(abs(MICROS_PER_SEC-max_micros), abs(MICROS_PER_SEC-min_micros)));
            dispersion  = (uint32_t)(disp * 65536.0);
#if 0
            ESP_LOGI(TAG, "min: %llu max: %llu jitter: %llu sats: %d fix: %d valid_count: %u valid_in: %u valid: %s",
                     min_micros, max_micros, max_micros - min_micros, gps_status.sat_count, gps_status.fix_quality, valid_count, valid_in, valid ? "true" : "false");
#endif
        }
        last_seconds = seconds;


    }
}

void start_gps()
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    xTaskCreatePinnedToCore(gps_task, "gps_task", 1024*2, NULL, GPS_TASK_PRI, NULL, 1);

}
