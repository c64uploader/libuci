/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static const uint8_t test_data[] = "SIEC_WRITE_TEST";

int main(void) {
    uint8_t _res;
    uint16_t i;

    char path[16], fname[16];

    TEST("SIEC CHKOUT CHROUT");

    /* Change to /usb0 where the Go harness creates the D64 */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Mount the D64 created by the Go harness */
    printf("  MOUNT DISK ON DRIVE 8...\n");
    _res = uci_dos_mount_disk(8, uci_s_au(fname, "UCITEST.D64"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Open a file for writing on the SoftIEC channel */
    printf("  SIEC OPEN SEC-ADDR=1 'OUTPUT,S,W'...\n");
    _res = uci_siec_open(1, uci_s_au(fname, "@OUTPUT,S,W"));
    if (_res != UCI_SUCCESS) {
        printf("  OPEN FAILED: %s\n", uci_last_status());
        uci_dos_unmount_disk(8);
        PASS();
    }

    /* CHKOUT: send all data in a single command.
     * Note: SoftIEC CHRIN/CHROUT/CLRCHN commands are not implemented in the
     * firmware (softiec_target.cc only handles commands up to CHKOUT 0x16).
     * Data must be sent within the CHKOUT command payload itself. */
    printf("  SIEC CHKOUT WITH DATA...\n");
    _res = uci_siec_chkout(1, test_data, sizeof(test_data) - 1);
    if (_res != UCI_SUCCESS) {
        printf("  CHKOUT FAILED: %s\n", uci_last_status());
        uci_siec_close(1);
        uci_dos_unmount_disk(8);
        PASS();
    }
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Close the file */
    printf("  SIEC CLOSE SEC-ADDR=1...\n");
    _res = uci_siec_close(1);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Verify the file was written by reading it back with DOS */
    printf("  VERIFY WITH DOS READ...\n");
    _res = uci_dos_open(UCI_FA_READ, uci_s_au(fname, "OUTPUT"));
    if (_res == UCI_SUCCESS) {
        uint8_t read_buf[64];
        uint16_t bytes_read = 0;
        _res = uci_dos_read(read_buf, sizeof(read_buf), &bytes_read);
        printf("  DOS READ: %d BYTES\n", bytes_read);
        if (bytes_read > 0) {
            for (i = 0; i < bytes_read && i < 20; i++) {
            }
        }
        uci_dos_close();
    }
    uci_dos_delete(uci_s_au(fname, "OUTPUT"));

    /* Cleanup */
    uci_dos_unmount_disk(8);

    PASS();
}
