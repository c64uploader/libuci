/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static const uint8_t data[] = "COPY TEST DATA 12345";
static uint8_t read_buf[64];
static uint16_t bytes_read;

int main(void) {
    uint8_t _res;

    char path[32], fname[20];

    TEST("DOS COPY");

    /* Ensure we are in a writable directory */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Create source file */
    printf("  WRITE SRC...\n");
    _res = uci_dos_open(UCI_FA_WRITE | UCI_FA_CREATE_ALWAYS, uci_s_au(fname, "UCITEST_SRC.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_write(data, sizeof(data) - 1);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Delete dest directory/file in case they exist from a failed run */
    uci_dos_delete(uci_s_au(path, "UCITEST_COPYDIR/UCITEST_SRC.DAT"));
    uci_dos_delete(uci_s_au(path, "UCITEST_COPYDIR"));

    /* Create destination directory */
    printf("  MKDIR...\n");
    _res = uci_dos_create_dir(uci_s_au(path, "UCITEST_COPYDIR"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Copy ucitest_src.dat into ucitest_copydir */
    printf("  COPY FILE...\n");
    _res = uci_dos_copy(uci_s_au(fname, "UCITEST_SRC.DAT"), uci_s_au(path, "/TEMP/UCITEST_COPYDIR"));
    printf("  COPY: RES=%d\n", _res);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Verify destination contents */
    printf("  VERIFY DEST...\n");
    _res = uci_dos_open(UCI_FA_READ, uci_s_au(path, "UCITEST_COPYDIR/UCITEST_SRC.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_read(read_buf, sizeof(read_buf), &bytes_read);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    printf("  READ: %d BYTES\n", bytes_read);

    if (bytes_read != sizeof(data) - 1) {
        FAIL("DATA_MISMATCH");
    }
    CHECK(memcmp(read_buf, data, sizeof(data) - 1) == 0, "MEM MISMATCH");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Clean up */
    printf("  CLEANUP...\n");
    uci_dos_delete(uci_s_au(path, "UCITEST_COPYDIR/UCITEST_SRC.DAT"));
    uci_dos_delete(uci_s_au(path, "UCITEST_COPYDIR"));
    uci_dos_delete(uci_s_au(fname, "UCITEST_SRC.DAT"));

    PASS();
}
