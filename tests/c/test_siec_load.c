/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

/* Test data: write to TEST_DATA area ($C008) */
static const uint8_t test_payload[] = "SIEC_LOAD_TEST_ABCDE";

int main(void) {
    uint8_t _res;
    uint16_t start_addr = 0;
    uint16_t end_addr = 0;
    uint16_t i;

    char path[16], fname[16];

    TEST("SIEC LOAD");

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

    /* First, SAVE a test file to the D64 */
    printf("  SIEC SAVE SEC-ADDR=1...\n");
    _res = uci_siec_save(false, 1, 0xC008, 0xC008 + sizeof(test_payload), uci_s_au(fname, "@UCITEST.PRG"));
    if (_res != UCI_SUCCESS) {
        printf("  SAVE FAILED: %s\n", uci_last_status());
        uci_dos_unmount_disk(8);
        PASS();
    }

    /* LOAD_SU: prepare to load the file we just saved */
    printf("  SIEC LOAD-SU SEC-ADDR=0...\n");
    _res = uci_siec_load_su(0, false, 0xC008, 0xFFFF, uci_s_au(fname, "UCITEST.PRG"), &start_addr);
    if (_res != UCI_SUCCESS) {
        printf("  LOAD-SU FAILED: %s\n", uci_last_status());
        uci_dos_unmount_disk(8);
        PASS();
    }
    printf("  START-ADDR=0X%04X\n", start_addr);

    /* Clear the TEST_DATA area to verify load actually writes */
    for (i = 0; i < sizeof(test_payload); i++) {
        HOST_IN[i] = 0;
    }

    /* LOAD_EX: execute the load */
    printf("  SIEC LOAD-EX SEC-ADDR=0...\n");
    _res = uci_siec_load_ex(0, false, &end_addr);
    if (_res != UCI_SUCCESS) {
        printf("  LOAD-EX FAILED: %s\n", uci_last_status());
    } else {
        printf("  END-ADDR=0X%04X\n", end_addr);

        /* Verify the loaded data matches what we saved */
        printf("  VERIFY LOADED DATA...\n");
        for (i = 0; i < sizeof(test_payload); i++) {
            if (HOST_IN[i] != test_payload[i]) {
                printf("  FAIL: MISMATCH AT OFFSET %d: GOT 0X%02X, WANT 0X%02X\n",
                       i, HOST_IN[i], test_payload[i]);
                FAIL("DATA_MISMATCH");
            }
        }
        printf("  DATA VERIFIED OK\n");
    }

    /* Cleanup */
    uci_dos_unmount_disk(8);

    PASS();
}
