/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static uci_drive_entry_t entries[4];

int main(void) {
    uint8_t _res;
    uint8_t count = 0;

    TEST("CTRL DRVINFO");

    /* effective_id=0: get current drive info */
    printf("  DRVINFO EFFECTIVE-ID=0...\n");
    _res = uci_ctrl_get_drvinfo(0, entries, 4, &count);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    printf("  DRIVE-COUNT=%d\n", count);

    PASS();
}
