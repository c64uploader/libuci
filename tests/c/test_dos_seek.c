/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static uint8_t write_buf[256];
static uint8_t read_buf[64];
static uint16_t bytes_read;
static uint16_t i;

int main(void) {
    uint8_t _res;

    char path[16], fname[20];

    TEST("DOS SEEK");

    /* Ensure we are in a writable directory */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Fill write buffer with known pattern */
    printf("  WRITE 256 BYTES...\n");
    for (i = 0; i < 256; i++) {
        write_buf[i] = (uint8_t)i;
    }

    /* Create file with 256 bytes */
    _res = uci_dos_open(UCI_FA_WRITE | UCI_FA_CREATE_ALWAYS, uci_s_au(fname, "UCITEST_SK.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_write(write_buf, 256);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Reopen for reading */
    printf("  REOPEN...\n");
    _res = uci_dos_open(UCI_FA_READ, uci_s_au(fname, "UCITEST_SK.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Seek to offset 128 */
    printf("  SEEK 128...\n");
    _res = uci_dos_seek(128);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Read 64 bytes - should be bytes 128..191 */
    _res = uci_dos_read(read_buf, 64, &bytes_read);
    printf("  READ: %d BYTES\n", bytes_read);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    if (bytes_read != 64) {
        FAIL("DATA_MISMATCH");
    }

    /* Verify content matches offset 128 of write buffer */
    CHECK(memcmp((read_buf),(write_buf + 128),(64)) == 0, "MEM MISMATCH");

    /* Seek back to 0 and verify */
    printf("  SEEK 0...\n");
    _res = uci_dos_seek(0);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_read(read_buf, 4, &bytes_read);
    printf("  READ: %d BYTES\n", bytes_read);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    if (bytes_read != 4) {
        FAIL("DATA_MISMATCH");
    }

    /* First 4 bytes should be 0,1,2,3 */
    CHECK((read_buf[0]) == (0x00), "GOT %d EXPECTED %d");
    CHECK((read_buf[1]) == (0x01), "GOT %d EXPECTED %d");
    CHECK((read_buf[2]) == (0x02), "GOT %d EXPECTED %d");
    CHECK((read_buf[3]) == (0x03), "GOT %d EXPECTED %d");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Clean up */
    printf("  CLEANUP...\n");
    uci_dos_delete(uci_s_au(fname, "UCITEST_SK.DAT"));

    PASS();
}
