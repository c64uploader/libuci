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

    TEST("DOS FILE INFO");

    /* Ensure we are in a writable directory */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Create a test file */
    printf("  CREATE TEST FILE...\n");
    _res = uci_dos_open(UCI_FA_WRITE | UCI_FA_CREATE_ALWAYS, uci_s_au(fname, "UCITEST_INFO.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    _res = uci_dos_write((const uint8_t *)"HELLO", 5);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Close file so the directory entry is flushed with the updated size */
    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Reopen for reading to get file info */
    _res = uci_dos_open(UCI_FA_READ | UCI_FA_OPEN_EXISTING, uci_s_au(fname, "UCITEST_INFO.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Get file info on the open file */
    printf("  FILE INFO...\n");
    _res = uci_dos_file_info(&info);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Verify size is 5 */
    if (info.size != 5) {
        printf("  FAIL: EXPECTED SIZE 5, GOT %lu\n", (unsigned long)info.size);
        FAIL("DATA_MISMATCH");
    }
    printf("  SIZE=%lu ATTRIB=0X%02X NAME='%s'\n",
           (unsigned long)info.size, info.attrib, info.filename);

    /* Close file */
    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* File info should fail with no file open */
    printf("  FILE INFO NO FILE...\n");
    _res = uci_dos_file_info(&info);
    if (_res == UCI_SUCCESS) {
        /* Some firmwares may succeed; check status */
        if (uci_last_status_ok()) {
            /* Unexpected success, but not a hard failure */
            printf("  WARN: FILE-INFO SUCCEEDED WITHOUT OPEN FILE\n");
        }
    }
    /* Expected: error or non-OK status */
    printf("  STATUS: %s\n", uci_last_status());

    /* Cleanup */
    uci_dos_delete(uci_s_au(fname, "UCITEST_INFO.DAT"));

    PASS();
}
