/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char buf[128];
static uci_sid_info_t sid_info;

int main(void) {
    uint8_t _res;

    TEST("CTRL HWINFO");

    /* Device 0: product string */
    printf("  HWINFO DEVICE 0 (PRODUCT)...\n");
    _res = uci_ctrl_get_hwinfo(UCI_HWINFO_PRODUCT, buf, sizeof(buf));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");
    CHECK((buf) && (buf)[0], "EMPTY STRING");
    printf("  PRODUCT: %s\n", buf);

    /* Device 1: SID configuration via typed struct */
    printf("  HWINFO DEVICE 1 (SID INFO)...\n");
    _res = uci_ctrl_get_sid_info(&sid_info);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");
    printf("  SID COUNT: %d\n", sid_info.count);

    PASS();
}
