/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static uci_file_info_t info;

int main(void) {
    uint8_t _res;

    char path[16], fname[20];

    TEST("DOS FILE STAT");

    /* Ensure we are in a writable directory */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Create a test file */
    printf("  CREATE TEST FILE...\n");
    _res = uci_dos_open(UCI_FA_WRITE | UCI_FA_CREATE_ALWAYS, uci_s_au(fname, "UCITEST_STAT.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_dos_write((const uint8_t *)"STAT_TEST_12345", 15);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Stat the file by name (no open required) */
    printf("  FILE STAT...\n");
    _res = uci_dos_file_stat(uci_s_au(fname, "UCITEST_STAT.DAT"), &info);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Verify size is 15 */
    if (info.size != 15) {
        printf("  FAIL: EXPECTED SIZE 15, GOT %lu\n", (unsigned long)info.size);
        FAIL("DATA_MISMATCH");
    }
    printf("  SIZE=%lu ATTRIB=0X%02X NAME='%s'\n",
           (unsigned long)info.size, info.attrib, info.filename);

    /* Stat a nonexistent file - expect failure */
    printf("  STAT NONEXISTENT...\n");
    _res = uci_dos_file_stat(uci_s_au(fname, "NO_SUCH_FILE_XYZ.DAT"), &info);
    if (_res == UCI_SUCCESS) {
        FAIL("UNEXPECTED_OK");
    }
    printf("  STATUS: %s\n", uci_last_status());

    /* Cleanup */
    uci_dos_delete(uci_s_au(fname, "UCITEST_STAT.DAT"));

    PASS();
}
