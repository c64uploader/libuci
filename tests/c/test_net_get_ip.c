/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static uint8_t ip[4], netmask[4], gateway[4];
static uint8_t iface;
static bool found;

int main(void) {
    uint8_t _res;

    TEST("NET GET IP");

    /* Try interfaces 0-3 until we find one with an IP */
    found = false;

    for (iface = 0; iface < 4; iface++) {
        printf("  TRYING IFACE %d...\n", iface);
        _res = uci_net_get_ip(iface, ip, netmask, gateway);
        printf("  GET IP: RES=%d IP=%d.%d.%d.%d\n",
               _res, ip[0], ip[1], ip[2], ip[3]);
        if (_res == UCI_SUCCESS && (ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0)) {
            found = true;
            break;
        }
    }

    if (!found) {
        FAIL("NET_ERROR");
    }

    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* IP should not be 0.0.0.0 */
    if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
        FAIL("DATA_MISMATCH");
    }

    /* Write results */

    PASS();
}
