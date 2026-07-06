/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static char filename[64];
static uint8_t attrib;
static uint16_t entry_count;

int main(void) {
    uint8_t _res;

    TEST("DOS LIST DIR");

    /* Open root directory */
    printf("  CHDIR /...\n");
    _res = uci_dos_change_dir("/");
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    printf("  OPEN DIR...\n");
    _res = uci_dos_open_dir();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Read entries */
    entry_count = 0;
    while ((_res = uci_dos_read_dir(&attrib, filename, sizeof(filename))) == UCI_SUCCESS) {
        entry_count++;
        /* Filename should be non-empty */
        if (filename[0] == '\0') {
            FAIL("DIR_UNEXPECTED");
        }
        /* Store first entry attrib and name for debugging */
        if (entry_count == 1) {
        }
        /* Limit to avoid infinite loop */
        if (entry_count > 100) break;
    }
    if (_res != UCI_ERR_END_OF_LISTING) {
        printf("  READ DIR ERROR: %d\n", _res);
        FAIL("DIR_UNEXPECTED");
    }

    /* Should have found at least one entry */
    printf("  ENTRIES: %d\n", entry_count);
    if (entry_count == 0) {
        FAIL("DIR_UNEXPECTED");
    }

    PASS();
}
