/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char time_buf[64];

int main(void) {
    uint8_t _res;

    TEST("DOS GET TIME");

    /* Get time in format 0 (BCD) */
    printf("  TIME BCD...\n");
    _res = uci_dos_get_time(0, time_buf, sizeof(time_buf));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Buffer should be non-empty */
    if (time_buf[0] == '\0') {
        FAIL("DATA_MISMATCH");
    }
    printf("  BCD: %s\n", time_buf);

    /* Get time in format 1 (string) */
    printf("  TIME STR...\n");
    _res = uci_dos_get_time(1, time_buf, sizeof(time_buf));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    if (time_buf[0] == '\0') {
        FAIL("DATA_MISMATCH");
    }
    printf("  STR: %s\n", time_buf);

    PASS();
}
