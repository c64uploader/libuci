/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char time_buf[64];

int main(void) {
    uint8_t _res;

    TEST("DOS SET TIME");

    /* Set time to 2026-01-15 12:30:45 */
    printf("  SET TIME 2026-01-15 12:30:45...\n");
    _res = uci_dos_set_time(2026, 1, 15, 12, 30, 45);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Read back time to verify */
    printf("  GET TIME...\n");
    _res = uci_dos_get_time(0, time_buf, sizeof(time_buf));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    printf("  TIME: %s\n", time_buf);

    /* Verify it contains "2026" */
    if (time_buf[0] == '\0') {
        FAIL("DATA_MISMATCH");
    }

    /* Verify the year portion */
    if (time_buf[0] != '2' || time_buf[1] != '0' ||
        time_buf[2] != '2' || time_buf[3] != '6') {
        printf("  FAIL: EXPECTED YEAR 2026, GOT %.4S\n", time_buf);
        FAIL("DATA_MISMATCH");
    }

    PASS();
}
