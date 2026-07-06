/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/02_http_client/http_client.c - Tiny HTTP GET client implementation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "uci.h"
#include "uci_codec.h"
#include "http_client.h"

uci_err_t http_get(const char *host, uint16_t port, const char *path,
                 char *buf, uint16_t max_len, char **body, uint16_t *status_code) {
    uint8_t socket_id;
    uint16_t req_len;
    uint16_t bytes_rw;
    uint16_t total;
    uci_err_t res;
    char *p;

    if (!host || !path || !buf || max_len < 64) {
        return UCI_ERR_PARAM;
    }

    /* Check request string size fits in buffer */
    req_len = 4 + (uint16_t)strlen(path) + 17 + (uint16_t)strlen(host) + 23;
    if (req_len >= max_len) {
        return UCI_ERR_PARAM;
    }

    /* Convert hostname to ASCII in caller's buffer for TCP connect */
    uci_s_a(buf, host);
    res = uci_net_open_tcp(buf, port, &socket_id);
    if (res != UCI_SUCCESS) {
        return res;
    }

    /* Build HTTP GET request string in caller's buffer */
    strcpy(buf, "GET ");
    strcat(buf, path);
    strcat(buf, " HTTP/1.0\r\nHost: ");
    strcat(buf, host);
    strcat(buf, "\r\nConnection: close\r\n\r\n");

    /* Convert HTTP request from compiler-native to ASCII in place */
    uci_s_a(buf, buf);
    req_len = (uint16_t)strlen(buf);

    /* Send HTTP request */
    res = uci_net_write_socket(socket_id, (const uint8_t *)buf, req_len, &bytes_rw);
    if (res != UCI_SUCCESS) {
        uci_net_close_socket(socket_id);
        return res;
    }

    /* Read response into caller's buffer until socket EOF or buffer full */
    total = 0;
    while (total < max_len - 1) {
        uint16_t got = 0;
        res = uci_net_read_socket(socket_id, (uint8_t *)buf + total,
                                  max_len - total - 1, &got);
        if (got > 0) {
            total += got;
        }
        if (got == 0 || res != UCI_SUCCESS) {
            break;
        }
    }

    uci_net_close_socket(socket_id);
    buf[total] = '\0';

    if (total == 0) {
        return UCI_ERR_TIMEOUT;
    }

    /* Parse HTTP status code (e.g. "HTTP/1.1 200 OK") */
    if (status_code) {
        *status_code = 0;
        p = strchr(buf, ' ');
        if (p && p[1] >= '0' && p[1] <= '9') {
            *status_code = (uint16_t)((p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0'));
        }
    }

    /* Locate HTTP body start (after header separator \r\n\r\n or \n\n) */
    if (body) {
        p = strstr(buf, "\r\n\r\n");
        if (p) {
            *body = p + 4;
        } else {
            p = strstr(buf, "\n\n");
            *body = p ? (p + 2) : buf;
        }
    }

    return UCI_SUCCESS;
}
