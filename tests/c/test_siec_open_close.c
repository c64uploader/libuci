/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

int main(void) {
    uint8_t _res;
    char path[16], fname[16];

    TEST("SIEC OPEN CLOSE");

    /* Change to /temp where the Go harness creates the D64 */
    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Mount the D64 created by the Go harness */
    printf("  MOUNT DISK ON DRIVE 8...\n");
    _res = uci_dos_mount_disk(8, uci_s_au(fname, "UCITEST.D64"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Open a file on the SoftIEC channel */
    printf("  SIEC OPEN SEC-ADDR=2 'UCITEST'...\n");
    _res = uci_siec_open(2, uci_s_au(fname, "UCITEST"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Close the channel */
    printf("  SIEC CLOSE SEC-ADDR=2...\n");
    _res = uci_siec_close(2);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Cleanup */
    uci_dos_unmount_disk(8);

    PASS();
}
