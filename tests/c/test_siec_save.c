/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

/* Test data to save: write to TEST_DATA area ($C008) before running */
static const uint8_t test_payload[] = "SIEC_SAVE_TEST_12345";

int main(void) {
    uint8_t _res;
    uint16_t i;
    const char *status;

    char path[16], fname[16];

    TEST("SIEC SAVE");

    /* Change to /usb0 where the Go harness creates the D64 */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Mount the D64 created by the Go harness */
    printf("  MOUNT DISK ON DRIVE 8...\n");
    _res = uci_dos_mount_disk(8, uci_s_au(fname, "UCITEST.D64"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Write test payload to TEST_DATA area ($C008) */
    printf("  WRITE PAYLOAD TO $C008...\n");
    for (i = 0; i < sizeof(test_payload); i++) {
        HOST_IN[i] = test_payload[i];
    }

    /* SAVE: save memory range to file on the D64.
     * We prefix the filename with '@' to replace/overwrite the file if it exists.
     */
    printf("  SIEC SAVE SEC-ADDR=1...\n");
    _res = uci_siec_save(false, 1, 0xC008, 0xC008 + sizeof(test_payload), uci_s_au(fname, "@UCITEST"));
    status = uci_last_status();
    printf("  RESULT: %d, STATUS: '%s'\n", _res, status);

    /* Write raw status bytes to details for debugging */

    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Cleanup */
    uci_dos_unmount_disk(8);

    PASS();
}
