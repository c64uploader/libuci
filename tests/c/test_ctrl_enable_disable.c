/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static bool powered;

int main(void) {
    uint8_t _res;

    TEST("CTRL ENABLE DISABLE");

    /* Enable drive A */
    printf("  ENABLE A...\n");
    _res = uci_ctrl_enable_disk_a();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Check power state reflects enabled */
    _res = uci_ctrl_disk_a_power(&powered);
    printf("  A POWER: %d\n", powered);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Disable drive A */
    printf("  DISABLE A...\n");
    _res = uci_ctrl_disable_disk_a();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Check power state */
    _res = uci_ctrl_disk_a_power(&powered);
    printf("  A POWER: %d\n", powered);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Enable drive B */
    printf("  ENABLE B...\n");
    _res = uci_ctrl_enable_disk_b();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_ctrl_disk_b_power(&powered);
    printf("  B POWER: %d\n", powered);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Disable drive B */
    printf("  DISABLE B...\n");
    _res = uci_ctrl_disable_disk_b();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_ctrl_disk_b_power(&powered);
    printf("  B POWER: %d\n", powered);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Restore drive A to enabled state */
    uci_ctrl_enable_disk_a();

    PASS();
}
