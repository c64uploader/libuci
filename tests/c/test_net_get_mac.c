/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static uint8_t mac[6];

int main(void) {
    uint8_t _res;

    TEST("NET GET MAC");

    printf("  READING MAC IFACE 0...\n");
    _res = uci_net_get_mac(0, mac);
    printf("  GET MAC: RES=%d\n", _res);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    printf("  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* MAC should not be all-zero */
    if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 &&
        mac[3] == 0 && mac[4] == 0 && mac[5] == 0) {
        FAIL("DATA_MISMATCH");
    }

    /* MAC should not be all-FF */
    if (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF &&
        mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF) {
        FAIL("DATA_MISMATCH");
    }

    /* Write MAC to results */

    PASS();
}
