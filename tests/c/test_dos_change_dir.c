/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static char path_buf[128];

int main(void) {
    uint8_t _res;
    char dir[32];

    TEST("DOS CHANGE DIR");

    /* Get initial path */
    printf("  GET PATH...\n");
    _res = uci_dos_get_path(path_buf, sizeof(path_buf));
    printf("  PATH: %s\n", path_buf);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Change to root */
    printf("  CHDIR /...\n");
    _res = uci_dos_change_dir(uci_s_au(dir, "/"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Verify path is now "/" */
    _res = uci_dos_get_path(path_buf, sizeof(path_buf));
    printf("  PATH: %s\n", path_buf);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    /* Path should start with / */
    if (path_buf[0] != '/') {
        FAIL("UNEXPECTED_STATUS");
    }

    /* Test invalid path returns error */
    printf("  CHDIR /BAD...\n");
    _res = uci_dos_change_dir(uci_s_au(dir, "/NONEXISTENT_DIR_XYZ_12345"));
    printf("  RES: %d (EXPECT FAIL)\n", _res);
    if (_res == UCI_SUCCESS) {
        /* Unexpected success for invalid path */
        FAIL("UNEXPECTED_OK");
    }
    CHECK(uci_has_error(), "EXPECTED ERROR FLAG");

    /* Clean up */
    uci_reset();

    PASS();
}
