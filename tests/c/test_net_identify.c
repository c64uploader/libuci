/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char buf[128];

int main(void) {
    uint8_t _res;

    TEST("NET IDENTIFY");

    printf("  IDENTIFYING...\n");
    _res = uci_net_identify(buf, sizeof(buf));
    printf("  IDENTIFY: RES=%d\n", _res);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");
    CHECK((buf) && (buf)[0], "EMPTY STRING");

    printf("  FIRMWARE: %s\n", buf);

    PASS();
}
