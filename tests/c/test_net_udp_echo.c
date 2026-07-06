/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * test_net_udp_echo.c - UDP echo test.
 *
 * The Go harness starts a UDP echo server and writes its IP + port
 * into $C008-$C00F before running this test:
 *   $C008-$C00B : server IP (4 bytes);
 *   $C00C-$C00D : TCP port (2 bytes, big-endian) - not used here
 *   $C00E-$C00F : UDP port (2 bytes, big-endian);
 */
#include "harness.h"
#include <string.h>

static uint8_t socket_id;
static uint8_t recv_buf[129];
static uint16_t bytes_read;
static uint16_t bytes_written;
static uint8_t server_ip[4];
static uint16_t port;
static char host[24];
static uint8_t i;
static char *p;
static uint8_t val;

static const char *test_msg = "HELLO FROM C64 UDP!";
#define TEST_MSG_LEN 19

int main(void) {
    uint8_t _res;

    TEST("NET UDP ECHO");

    /* Read server IP from $C006-$C009 */
    server_ip[0] = HOST_IN[0];
    server_ip[1] = HOST_IN[1];
    server_ip[2] = HOST_IN[2];
    server_ip[3] = HOST_IN[3];

    /* Read UDP server port from $C00C-$C00D (big-endian) */
    port = ((uint16_t)(HOST_IN[6]) << 8) | HOST_IN[7];

    printf("  SERVER: %d.%d.%d.%d:%d\n",
           server_ip[0], server_ip[1], server_ip[2], server_ip[3], port);

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

    /* Open UDP socket */
    printf("  OPENING UDP %s:%d...\n", host, port);
    _res = uci_net_open_udp(host, port, &socket_id);
    printf("  OPEN UDP: RES=%d STATUS=%s\n", _res, uci_last_status());
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    if (socket_id == 0) {
        FAIL("NET_ERROR");
    }
    printf("  SOCKET ID=%d\n", socket_id);

    /* Send test datagram */
    printf("  SENDING %d BYTES...\n", TEST_MSG_LEN);
    _res = uci_net_write_socket(socket_id, (const uint8_t *)test_msg, TEST_MSG_LEN, &bytes_written);
    printf("  WRITE: RES=%d WRITTEN=%d\n", _res, bytes_written);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    if (bytes_written != TEST_MSG_LEN) {
        printf("  WROTE %d, EXPECTED %d\n", bytes_written, TEST_MSG_LEN);
        FAIL("NET_ERROR");
    }

    /* Read echoed response */
    printf("  READING ECHO...\n");
    _res = uci_net_read_socket(socket_id, recv_buf, sizeof(recv_buf), &bytes_read);
    printf("  READ: RES=%d READ=%d\n", _res, bytes_read);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    if (bytes_read != TEST_MSG_LEN) {
        printf("  READ %d, EXPECTED %d\n", bytes_read, TEST_MSG_LEN);
        FAIL("DATA_MISMATCH");
    }

    /* Verify echoed data matches sent data */
    if (memcmp(recv_buf, test_msg, TEST_MSG_LEN) != 0) {
        printf("  ECHO MISMATCH\n");
        printf("    SENT: ");
        for (i = 0; i < TEST_MSG_LEN; i++) printf("%C", test_msg[i]);
        printf("\n    GOT:  ");
        for (i = 0; i < TEST_MSG_LEN; i++) printf("%C", recv_buf[i]);
        printf("\n");
        FAIL("DATA_MISMATCH");
    }

    /* Close socket */
    _res = uci_net_close_socket(socket_id);
    printf("  CLOSE: RES=%d\n", _res);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    PASS();
}
