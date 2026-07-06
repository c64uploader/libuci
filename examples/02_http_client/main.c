/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/02_http_client/main.c - HTTP Client & JSON Parser
 *
 * Concept Primer:
 *   HTTP (Hypertext Transfer Protocol) is a text-based request/response protocol.
 *   This example opens a TCP connection to an HTTP server (port 80), sends a standard
 *   "GET /json HTTP/1.0" request, reads the HTTP response header and JSON payload,
 *   and extracts fields like country, city, and IP address.
 *
 * Demonstrates:
 *   - Reusable http_get() client module
 *   - Reusable json_get() parser module
 *   - uci_s_p() - convert ASCII to PETSCII for display
 */

#include <stdio.h>
#include <stdint.h>
#include "uci.h"
#include "uci_codec.h"
#include "http_client.h"
#include "json.h"

int main(void) {
    static char buf[512];
    static char value[32];
    uint16_t status_code = 0;
    uci_err_t res;
    char *body = NULL;

    static const struct { const char *key; const char *label; } fields[] = {
        {"country",    "\n  COUNTRY:"},
        {"regionName", "  REGION: "},
        {"city",       "  CITY:   "},
        {"timezone",   "  TZ:     "},
        {"isp",        "  ISP:    "},
        {"query",      "  IP:     "}
    };
    uint8_t i;

    printf("*** HTTP CLIENT ***\n\n");

    /* Initialize UCI hardware */
    if (uci_init() != UCI_SUCCESS) {
        printf("UCI INIT FAILED!\n");
        return 1;
    }

    printf("CONNECTING TO IP-API.COM...\n");
    res = http_get("ip-api.com", 80, "/json", buf, sizeof(buf), &body, &status_code);
    if (res != UCI_SUCCESS) {
        printf("HTTP REQUEST FAILED: %s\n", uci_last_status());
        uci_reset();
        return 1;
    }

    if (status_code != 200) {
        printf("HTTP SERVER ERROR: STATUS %u\n", status_code);
        uci_reset();
        return 1;
    }

    /* Convert ASCII response body to PETSCII for C64 display */
    uci_s_p(body, body);

    /* Parse and display location info */
    for (i = 0; i < 6; i++) {
        if (json_get(body, fields[i].key, value, sizeof(value))) {
            uci_s_au(value, value);
            printf("%s %s\n", fields[i].label, value);
        }
    }

    printf("\nDONE!\n");
    uci_reset();
    return 0;
}
