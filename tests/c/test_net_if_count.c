/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char names[128];

int main(void) {
    uint8_t _res;
    uint8_t count = 0;

    TEST("NET INTERFACE COUNT");

    printf("  GET INTERFACE COUNT...\n");
    _res = uci_net_get_interface_count(&count, names, sizeof(names));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    printf("  COUNT=%d NAMES='%s'\n", count, names);

    /* Should have at least 1 interface */
    if (count < 1) {
        printf("  FAIL: EXPECTED >= 1 INTERFACE, GOT %d\n", count);
        FAIL("DATA_MISMATCH");
    }

    PASS();
}
