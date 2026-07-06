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

    TEST("DOS MOUNT UNMOUNT");

    /* Change directory to /temp where the Go harness creates the D64 file */
    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Mount a D64 image to drive 8.
     * The Go harness creates this D64 file before running the test. */
    printf("  MOUNT D64...\n");
    _res = uci_dos_mount_disk(8, uci_s_au(fname, "UCITEST.D64"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Unmount */
    printf("  UNMOUNT...\n");
    _res = uci_dos_unmount_disk(8);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Test invalid drive ID - unmounting an already unmounted drive should succeed or fail gracefully.
     * We test that the function doesn't crash. */
    printf("  UNMOUNT AGAIN...\n");
    _res = uci_dos_unmount_disk(8);
    /* Don't assert on the result - the drive is already unmounted */

    /* Clean up */
    printf("  CLEANUP...\n");
    uci_reset();

    PASS();
}
