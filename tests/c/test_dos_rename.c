/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static const uint8_t data[] = "RENAME TEST CONTENT";
static uint8_t read_buf[64];
static uint16_t bytes_read;

int main(void) {
    uint8_t _res;
    char path[16], old_name[20], new_name[20];

    TEST("DOS RENAME");

    /* Ensure we are in a writable directory */
    printf("  CHDIR /TEMP...\n");
    _res = uci_dos_change_dir(uci_s_au(path, "/TEMP"));
    printf("  CHDIR RES=%d STATUS='%s'\n", _res, uci_last_status());

    /* Create file with old name */
    printf("  WRITE OLD FILE...\n");
    _res = uci_dos_open(UCI_FA_WRITE | UCI_FA_CREATE_ALWAYS, uci_s_au(old_name, "UCITEST_OLD.DAT"));
    printf("  OPEN RES=%d STATUS='%s'\n", _res, uci_last_status());
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_write(data, sizeof(data) - 1);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Delete target name in case it exists from a previous failed run */
    uci_dos_delete(uci_s_au(new_name, "UCITEST_NEW.DAT"));

    /* Rename */
    printf("  RENAME...\n");
    _res = uci_dos_rename(uci_s_au(old_name, "UCITEST_OLD.DAT"), uci_s_au(new_name, "UCITEST_NEW.DAT"));
    printf("  RENAME RES=%d STATUS='%s'\n", _res, uci_last_status());
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Verify old name fails */
    printf("  VERIFY OLD GONE...\n");
    _res = uci_dos_open(UCI_FA_READ, uci_s_au(old_name, "UCITEST_OLD.DAT"));
    if (_res == UCI_SUCCESS) {
        uci_dos_close();
        FAIL("UNEXPECTED_OK");
    }
    uci_reset();

    /* Verify new name succeeds and contains correct data */
    printf("  VERIFY NEW...\n");
    _res = uci_dos_open(UCI_FA_READ, uci_s_au(new_name, "UCITEST_NEW.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_read(read_buf, sizeof(read_buf), &bytes_read);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    if (bytes_read != sizeof(data) - 1) {
        FAIL("DATA_MISMATCH");
    }
    CHECK(memcmp(read_buf, data, sizeof(data) - 1) == 0, "MEM MISMATCH");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Clean up */
    printf("  CLEANUP...\n");
    uci_dos_delete(uci_s_au(new_name, "UCITEST_NEW.DAT"));

    PASS();
}
