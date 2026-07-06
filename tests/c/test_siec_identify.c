/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char buf[128];

int main(void) {
    uint8_t _res;

    TEST("SIEC IDENTIFY");

    printf("  SIEC IDENTIFY...\n");
    _res = uci_siec_identify(buf, sizeof(buf));
    printf("  RESULT: %s\n", buf);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");
    CHECK((buf) && (buf)[0], "EMPTY STRING");

    /* Should contain "SOFTWARE IEC" */
    if (buf[0] == '\0') {
        FAIL("DATA_MISMATCH");
    }

    PASS();
}
