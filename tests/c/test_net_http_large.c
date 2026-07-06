/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * test_net_http_large.c - Simulate HTTP request/response pattern.
 *
 * The Go harness starts a TCP server that:
 *   1. Accepts a connection
 *   2. Reads the HTTP request (any data the C64 sends)
 *   3. Sends a large HTTP response (~3000 bytes, forcing multiple reads)
 *   4. Closes the connection
 *
 * The C64 sends an HTTP GET, then reads in chunks until the connection
 * closes, proving that the send-then-read-repeatedly pattern works for
 * responses larger than a single read buffer.
 *
 * Server info written by Go harness:
 *   $C008-$C00B : server IP (4 bytes);
 *   $C00C-$C00D : server port (2 bytes, big-endian);
 */
#include "harness.h"
#include "uci_codec.h"
#include <string.h>
#include "uci.h"

static uint8_t socket_id;
static uint8_t recv_buf[256];
static uint16_t bytes_read;
static uint16_t total_read;
static uint8_t server_ip[4];
static uint16_t port;
static char host[24];
static uint8_t i;
static char *p;
static uint8_t val;

/* HTTP GET request sent to the server */
static char http_get[80];
static uint16_t http_get_len;

#define BUF_SIZE 256
#define MIN_EXPECTED 2000

int main(void) {
    uint8_t _res;

    TEST("NET HTTP LARGE (2000+ BYTES)");
    CHECK(uci_init() == UCI_SUCCESS, "uci_init");

    /* Read server IP from $C006-$C009 */
    server_ip[0] = HOST_IN[0];
    server_ip[1] = HOST_IN[1];
    server_ip[2] = HOST_IN[2];
    server_ip[3] = HOST_IN[3];

    /* Read server port from $C00A-$C00B (big-endian) */
    port = ((uint16_t)(HOST_IN[4]) << 8) | HOST_IN[5];

    printf("  SERVER: %d.%d.%d.%d:%u\n",
           server_ip[0], server_ip[1], server_ip[2], server_ip[3],
           (unsigned int)port);

    /* Build hostname string "X.X.X.X" */
    p = host;
    for (i = 0; i < 4; i++) {
        val = server_ip[i];
        if (val >= 100) { *p++ = '0' + val / 100; val %= 100; *p++ = '0' + val / 10; *p++ = '0' + val % 10; }
        else if (val >= 10) { *p++ = '0' + val / 10; *p++ = '0' + val % 10; }
        else { *p++ = '0' + val; }
        if (i < 3) *p++ = '.';
    }
    *p = '\0';

    /* Open TCP connection to server */
    printf("  OPENING TCP %s:%u...\n", host, (unsigned int)port);
    _res = uci_net_open_tcp(host, port, &socket_id);
    printf("  OPEN TCP: RES=%d STATUS=%s\n", _res, uci_last_status());
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    if (socket_id == 0) {
        FAIL("NET_ERROR");
    }
    printf("  SOCKET ID=%d\n", socket_id);

    /* Build HTTP GET request with proper ASCII encoding.
     * Under cc65, character literals compile to PETSCII - convert to
     * ASCII once so the server receives real HTTP bytes. */
    http_get_len = (uint16_t)strlen(
        uci_s_a(http_get, "GET / HTTP/1.0\r\n"
                         "Host: localhost\r\n"
                         "Connection: close\r\n"
                         "\r\n"));

    /* Step 1: Send HTTP GET request (now in real ASCII) */
    printf("  SENDING HTTP GET (%d BYTES)...\n", http_get_len);
    _res = uci_net_write_socket(socket_id, (uint8_t *)http_get, http_get_len, &bytes_read);
    printf("  WRITE: RES=%d WRITTEN=%d STATUS=%s\n", _res, bytes_read, uci_last_status());
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    if (bytes_read != http_get_len) {
        printf("  WROTE %d, EXPECTED %d\n", bytes_read, http_get_len);
        FAIL("WRITE_INCOMPLETE");
    }

    /* Step 2: Read response in chunks until connection closed */
    printf("  READING RESPONSE...\n");
    total_read = 0;
    while (1) {
        bytes_read = 0;
        _res = uci_net_read_socket(socket_id, recv_buf, BUF_SIZE, &bytes_read);

        printf("  READ: RES=%d BYTES=%d TOTAL=%d STATUS=%s\n",
               _res, bytes_read, total_read + bytes_read, uci_last_status());

        if (bytes_read > 0) {
            total_read += bytes_read;
        }

        /* Stop if connection closed (status != ok) with no more data */
        if (bytes_read == 0) break;

        /* Also stop on non-success status codes that aren't "ok" */
        if (_res != UCI_SUCCESS && !uci_last_status_ok()) break;
    }

    printf("  TOTAL READ: %d\n", total_read);

    if (total_read < MIN_EXPECTED) {
        printf("  READ ONLY %d OF EXPECTED %d+ BYTES\n",
               total_read, MIN_EXPECTED);
        FAIL("DATA_TRUNCATED");
    }

    /* Close socket (may return non-zero if already closed by peer - that's ok) */
    _res = uci_net_close_socket(socket_id);
    printf("  CLOSE: RES=%d\n", _res);

    PASS();
}

