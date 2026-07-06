/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char path_buf[256];

int main(void) {
    uint8_t _res;

    TEST("DOS COPY HOME PATH");

    printf("  COPY HOME PATH...\n");
    _res = uci_dos_copy_home_path(path_buf, sizeof(path_buf));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Should return a non-empty path */
    CHECK((path_buf) && (path_buf)[0], "EMPTY STRING");
    printf("  HOME: %s\n", path_buf);

    /* Verify it starts with '/' (absolute path) */
    if (path_buf[0] != '/') {
        printf("  FAIL: EXPECTED ABSOLUTE PATH STARTING WITH '/', GOT '%s'\n", path_buf);
        FAIL("DATA_MISMATCH");
    }

    PASS();
}
