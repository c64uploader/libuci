/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static const uint8_t test_data[] = "HELLO LIBUCI TEST DATA FILE! 0123456789ABCDEF";
static uint8_t read_buf[256];
static uint16_t bytes_read;

int main(void) {
    uint8_t _res;

    char path[16], fname[16];

    TEST("DOS OPEN READ CLOSE");

    /* Ensure we are in a writable directory */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Create the test file first */
    printf("  WRITE FILE...\n");
    _res = uci_dos_open(UCI_FA_WRITE | UCI_FA_CREATE_ALWAYS, uci_s_au(fname, "UCITEST.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_dos_write(test_data, sizeof(test_data) - 1);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Open the test data file for reading */
    printf("  OPEN READ...\n");
    _res = uci_dos_open(UCI_FA_READ, uci_s_au(fname, "UCITEST.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Read some bytes */
    _res = uci_dos_read(read_buf, 64, &bytes_read);
    printf("  READ: %d BYTES\n", bytes_read);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* We should have read at least some bytes */
    if (bytes_read != sizeof(test_data) - 1) {
        FAIL("DATA_MISMATCH");
    }

    /* Close the file */
    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Clean up test file */
    printf("  CLEANUP...\n");
    uci_dos_delete(uci_s_au(fname, "UCITEST.DAT"));

    PASS();
}
