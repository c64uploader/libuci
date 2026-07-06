/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static const uint8_t test_data[] = "HELLO LIBUCI WRITE TEST! 0123456789ABCDEF";
static uint8_t read_buf[64];
static uint16_t bytes_read;

int main(void) {
    uint8_t _res;

    char path[16], fname[20];

    TEST("DOS WRITE");

    /* Ensure we are in a writable directory */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Create a new file and write test data */
    printf("  WRITE FILE...\n");
    _res = uci_dos_open(UCI_FA_WRITE | UCI_FA_CREATE_ALWAYS, uci_s_au(fname, "UCITEST_WR.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_dos_write(test_data, sizeof(test_data) - 1);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Reopen for read and verify contents */
    printf("  REOPEN...\n");
    _res = uci_dos_open(UCI_FA_READ, uci_s_au(fname, "UCITEST_WR.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Debug: check bytes_read before read call */
    printf("  BEFORE: %d\n", bytes_read);

    _res = uci_dos_read(read_buf, sizeof(read_buf), &bytes_read);
    printf("  READ: %d BYTES\n", bytes_read);
    printf("  AFTER: %d\n", bytes_read);
    printf("  RES: %d\n", _res);
    printf("  BUF0: %d\n", read_buf[0]);
    printf("  BUF1: %d\n", read_buf[1]);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Verify length matches */
    if (bytes_read != sizeof(test_data) - 1) {
        FAIL("DATA_MISMATCH");
    }

    /* Verify content matches */
    printf("  DATA OK\n");
    CHECK(memcmp(read_buf, test_data, sizeof(test_data) - 1) == 0, "MEM MISMATCH");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Clean up */
    printf("  CLEANUP...\n");
    uci_dos_delete(uci_s_au(fname, "UCITEST_WR.DAT"));

    PASS();
}
