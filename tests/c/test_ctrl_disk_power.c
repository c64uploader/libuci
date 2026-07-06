/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static bool powered;

int main(void) {
    uint8_t _res;

    TEST("CTRL DISK POWER");

    /* Query drive A power state */
    printf("  DRIVE A POWER...\n");
    _res = uci_ctrl_disk_a_power(&powered);
    printf("  DRIVE A: %d\n", powered);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* powered should be 0 or 1 (not garbage) */
    if (powered != 0 && powered != 1) {
        FAIL("DATA_MISMATCH");
    }

    /* Query drive B power state */
    printf("  DRIVE B POWER...\n");
    _res = uci_ctrl_disk_b_power(&powered);
    printf("  DRIVE B: %d\n", powered);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    if (powered != 0 && powered != 1) {
        FAIL("DATA_MISMATCH");
    }

    PASS();
}
