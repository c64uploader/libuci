/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char path_buf[128];

int main(void) {
    uint8_t _res;

    TEST("DOS GET PATH");

    /* Get path after init - should be non-empty */
    printf("  GET PATH...\n");
    _res = uci_dos_get_path(path_buf, sizeof(path_buf));
    printf("  PATH: %s\n", path_buf);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");
    CHECK((path_buf) && (path_buf)[0], "EMPTY STRING");

    /* Change to root and verify */
    printf("  CHDIR /...\n");
    _res = uci_dos_change_dir("/");
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_get_path(path_buf, sizeof(path_buf));
    printf("  PATH: %s\n", path_buf);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Path should start with / */
    if (path_buf[0] != '/') {
        printf("  BAD PREFIX\n");
        FAIL("UNEXPECTED_STATUS");
    }

    PASS();
}
