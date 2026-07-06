/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/03_ntp_rtc_sync/main.c - NTP Time Sync
 *
 * Concept Primer:
 *   NTP (Network Time Protocol) uses a 48-byte UDP packet (port 123).
 *   The client sets the first byte to 0x1B (LI=0, VN=3, Mode=3/client) and sends it.
 *   The NTP server responds with a 48-byte packet containing a transmit timestamp in bytes
 *   40-43 (32-bit seconds count since 1900). Subtracting 2,208,988,800 seconds converts
 *   the NTP timestamp to standard 1970 Unix epoch time.
 *
 * Demonstrates:
 *   - UDP networking (open, write, read, close)
 *   - Binary protocol (48-byte NTP packet)
 *   - DOS time functions (get_time, set_time)
 *   - Epoch-to-date conversion on 8-bit hardware
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "uci.h"
#include "uci_codec.h"

/* NTP packet buffer */
static uint8_t pkt[48];
static uint8_t socket_id;
static uint16_t bytes_rw;
static char time_buf[UCI_TIME_STR_MAX];

/* Days in each month (non-leap year) */
static const uint8_t days_in_month[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/* Convert Unix epoch to date/time fields */
static void epoch_to_datetime(uint32_t epoch,
                              uint16_t *year, uint8_t *month, uint8_t *day,
                              uint8_t *hour, uint8_t *min, uint8_t *sec) {
    uint32_t days, secs;
    uint32_t y = 1970;
    uint8_t m, dim;
    bool leap;

    secs = epoch % 86400UL;
    days = epoch / 86400UL;

    *hour = (uint8_t)(secs / 3600UL);
    *min  = (uint8_t)((secs % 3600UL) / 60UL);
    *sec  = (uint8_t)(secs % 60UL);

    /* Find the year */
    while (1) {
        leap = ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
        if (days < (leap ? 366 : 365)) break;
        days -= (leap ? 366 : 365);
        y++;
    }
    *year = (uint16_t)y;

    /* Find the month */
    for (m = 0; m < 12; m++) {
        dim = days_in_month[m];
        if (m == 1 && leap) dim = 29;
        if (days < dim) break;
        days -= dim;
    }
    *month = m + 1;
    *day   = (uint8_t)(days + 1);
}

int main(void) {
    uint8_t res;
    uint32_t ntp_time, unix_time;
    uint16_t year;
    uint8_t month, day, hour, min, sec;

    printf("*** NTP TIME SYNC ***\n\n");

    if (uci_init() != UCI_SUCCESS) {
        printf("UCI HARDWARE NOT FOUND!\n");
        return 1;
    }

    /* Open UDP socket on NTP port 123 (hostname before port) */
    printf("CONNECTING TO NTP SERVER...\n");
    {
        static char ascii_host[64];
        res = uci_net_open_udp(uci_s_a(ascii_host, "pool.ntp.org"), 123, &socket_id);
    }
    if (res != UCI_SUCCESS) {
        printf("UDP OPEN FAILED: %s\n", uci_last_status());
        return 1;
    }

    /* Build NTP client request: LI=0, VN=3, Mode=3 (client) */
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 0x1B;

    /* Send the request */
    printf("SENDING NTP REQUEST...\n");
    res = uci_net_write_socket(socket_id, pkt, 48, &bytes_rw);
    if (res != UCI_SUCCESS || bytes_rw < 48) {
        printf("SEND FAILED: %s\n", uci_last_status());
        uci_net_close_socket(socket_id);
        return 1;
    }

    /* Receive the response */
    res = uci_net_read_socket(socket_id, pkt, sizeof(pkt), &bytes_rw);
    uci_net_close_socket(socket_id);

    if (res != UCI_SUCCESS || bytes_rw < 48) {
        printf("NTP RESPONSE ERROR\n");
        return 1;
    }

    /* Extract transmit timestamp (bytes 40-43, seconds since 1900) */
    ntp_time = ((uint32_t)pkt[40] << 24) | ((uint32_t)pkt[41] << 16) |
               ((uint32_t)pkt[42] << 8)  |  (uint32_t)pkt[43];

    /* Convert NTP epoch (1900) to Unix epoch (1970) */
    unix_time = ntp_time - 2208988800UL;
    epoch_to_datetime(unix_time, &year, &month, &day, &hour, &min, &sec);

    printf("\nNTP UTC: %04u-%02u-%02u %02u:%02u:%02u\n",
           year, month, day, hour, min, sec);

    /* Update the hardware RTC */
    printf("SETTING HARDWARE RTC...\n");
    res = uci_dos_set_time(year, month, day, hour, min, sec);
    if (res != UCI_SUCCESS) {
        printf("RTC UPDATE FAILED: %s\n", uci_last_status());
        return 1;
    }

    /* Read back and display the new time */
    if (uci_dos_get_time(UCI_TIME_FORMAT_DATETIME, time_buf, sizeof(time_buf)) == UCI_SUCCESS) {
        uci_s_d(time_buf, time_buf);
        printf("RTC NOW: %s\n", time_buf);
    }

    printf("\nRTC SYNCED SUCCESSFULLY!\n");
    return 0;
}
