/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/01_sysinfo_dump/main.c - System Info
 *
 * Queries the Ultimate hardware and displays system information.
 *
 * API functions used:
 *   uci_init, uci_get_base
 *   uci_ctrl_get_hwinfo, uci_ctrl_get_sid_info
 *   uci_net_get_interface_count, uci_net_get_mac, uci_net_get_ip
 *   uci_dos_get_time
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "uci.h"
#include "uci_codec.h"

/* Byte to hex */
static void hex2(uint8_t val, char *dst) {
    static const char hex[] = "0123456789ABCDEF";
    dst[0] = hex[val >> 4];
    dst[1] = hex[val & 0x0F];
}

int main(void) {
    static char str_buf[UCI_PATH_STR_MAX];
    static char if_names[UCI_NAMES_MAX];
    static char val[32];
    static uint8_t mac[6];
    static uint8_t ip[4], mask[4], gw[4];
    static uci_sid_info_t sid_info;
    uint8_t if_count = 0;
    uint8_t i, j;

    printf("*** SYSTEM INFO ***\n\n");

    /* Initialize UCI */
    if (uci_init() != UCI_SUCCESS) {
        printf("UCI HARDWARE NOT FOUND\n");
        return 1;
    }

    /* Base address */
    printf("BASE ADDR:  $%04X\n", uci_get_base());

    /* Hardware Product */
    if (uci_ctrl_get_hwinfo(UCI_HWINFO_PRODUCT, str_buf, sizeof(str_buf)) == UCI_SUCCESS) {
        uci_s_d(str_buf, str_buf);
        printf("HARDWARE:   %s\n", str_buf);
    }

    /* SID info using typed structure */
    if (uci_ctrl_get_sid_info(&sid_info) == UCI_SUCCESS) {
        printf("SID:        %d SID", sid_info.count);
        if (sid_info.count != 1) printf("S");
        printf("\n");

        for (i = 0; i < sid_info.count; i++) {
            printf("  SID%d:     $%04X", i + 1, sid_info.sids[i].base_addr);
            if (sid_info.sids[i].enabled) printf(" (ENABLED)");
            printf("\n");
        }
    }

    /* Dynamic Network Interfaces Query */
    if (uci_net_get_interface_count(&if_count, if_names, sizeof(if_names)) == UCI_SUCCESS) {
        printf("NET IFACE:  %d INTERFACE(S)\n", if_count);
        for (i = 0; i < if_count; i++) {
            printf("--- IFACE %d ---\n", i);
            if (uci_net_get_mac(i, mac) == UCI_SUCCESS) {
                for (j = 0; j < 6; j++) {
                    hex2(mac[j], val + j * 3);
                    if (j < 5) val[j * 3 + 2] = ':';
                }
                val[17] = '\0';
                printf("MAC:        %s\n", val);
            }
            if (uci_net_get_ip(i, ip, mask, gw) == UCI_SUCCESS) {
                printf("IP ADDR:    %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
                printf("NETMASK:    %d.%d.%d.%d\n", mask[0], mask[1], mask[2], mask[3]);
                printf("GATEWAY:    %d.%d.%d.%d\n", gw[0], gw[1], gw[2], gw[3]);
            }
        }
    }

    /* RTC */
    if (uci_dos_get_time(UCI_TIME_FORMAT_DATETIME, str_buf, sizeof(str_buf)) == UCI_SUCCESS) {
        uci_s_d(str_buf, str_buf);
        printf("RTC:        %s\n", str_buf);
    }

    printf("\nDONE.\n");

    return 0;
}
