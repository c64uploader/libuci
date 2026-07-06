/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * test_net_tcp_echo_large.c - TCP large payload test.
 *
 * The Go harness starts a TCP server that writes a 600-byte payload and
 * closes the socket immediately. The C64 reads the payload and verifies
 * it byte-by-byte, proving the C64/Ultimate can receive more than 392
 * bytes over a single TCP connection.
 *
 * Server info written by Go harness:
 *   $C008-$C00B : server IP (4 bytes);
 *   $C00C-$C00D : server port (2 bytes, big-endian);
 */
#include "harness.h"
#include <string.h>
#include "uci.h"

static uint8_t socket_id;
static uint8_t recv_buf[600];
static uint16_t bytes_read;
static uint16_t total_read;
static uint8_t server_ip[4];
static uint16_t port;
static char host[24];
static uint8_t i;
static char *p;
static uint8_t val;

/* Expected payload - filled with index & 0xFF at runtime */
static uint8_t expected[600];

#define PAYLOAD_LEN 600

int main(void) {
    uint8_t _res;
    uint16_t j;
    uint16_t request_size;

    TEST("NET TCP LARGE (600 BYTES)");
    CHECK(uci_init() == UCI_SUCCESS, "uci_init");

    /* Fill expected buffer with index & 0xFF */
    for (j = 0; j < 600; j++) expected[j] = (uint8_t)(j & 0xFF);

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

    /* Wait for server to send us data and close */
    {
        volatile uint16_t d;
        for (d = 0; d < 30000; d++);
    }

    /* Read payload - may arrive in multiple chunks */
    printf("  READING PAYLOAD...\n");
    total_read = 0;
    while (total_read < PAYLOAD_LEN) {
        request_size = PAYLOAD_LEN - total_read;

        bytes_read = 0;
        _res = uci_net_read_socket(socket_id, recv_buf + total_read,
                                   request_size, &bytes_read);

        if (bytes_read > 0) {
            total_read += bytes_read;
            printf("  READ %d TOTAL=%d RES=%d STATUS=%s\n",
                   bytes_read, total_read, _res, uci_last_status());
        }

        /* If connection closed or error with no data, stop */
        if (bytes_read == 0) {
            printf("  NO MORE DATA (RES=%d STATUS=%s)\n",
                   _res, uci_last_status());
            break;
        }

        /* If connection closed (non-zero status but data returned), carry on */
        if (_res != UCI_SUCCESS && !uci_last_status_ok()) {
            printf("  CONNECTION CLOSED AFTER %d BYTES (STATUS=%s)\n",
                   total_read, uci_last_status());
            break;
        }
    }

    if (total_read == 0) {
        printf("  GOT ZERO BYTES!\n");
        FAIL("NET_ERROR");
    }

    printf("  TOTAL READ: %d\n", total_read);

    if (total_read < PAYLOAD_LEN) {
        printf("  READ ONLY %d OF %d BYTES\n", total_read, PAYLOAD_LEN);
        FAIL("DATA_TRUNCATED");
    }

    /* Verify payload matches expected pattern */
    printf("  VERIFYING FIRST %d BYTES...\n", PAYLOAD_LEN);
    for (j = 0; j < PAYLOAD_LEN; j++) {
        if (recv_buf[j] != expected[j]) {
            printf("  MISMATCH AT BYTE %d: GOT %02X, EXPECTED %02X\n",
                   j, recv_buf[j], expected[j]);
            if (j >= 4) {
                printf("  PREV: %02X %02X %02X %02X\n",
                       recv_buf[j-4], recv_buf[j-3], recv_buf[j-2], recv_buf[j-1]);
            }
            FAIL("DATA_MISMATCH");
        }
    }
    printf("  ALL %d BYTES MATCH!\n", PAYLOAD_LEN);

    /* Close socket */
    _res = uci_net_close_socket(socket_id);
    printf("  CLOSE: RES=%d\n", _res);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    PASS();
}

