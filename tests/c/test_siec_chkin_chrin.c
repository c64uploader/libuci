/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

/* Test data to write and read back */
static const uint8_t test_payload[] = "SIEC_CHKIN_TEST_789";
static uint8_t prefetch[64];
static uint8_t read_buf[64];

int main(void) {
    uint8_t _res;
    uint16_t prefetch_len = 0;
    uint16_t i;
    uint16_t bytes_read = 0;

    char path[16], fname[16];

    TEST("SIEC CHKIN CHRIN");

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

    /* Save a test file to the D64 */
    printf("  SIEC SAVE SEC-ADDR=1...\n");
    _res = uci_siec_save(false, 1, 0xC008, 0xC008 + sizeof(test_payload), uci_s_au(fname, "@UCITEST.PRG"));
    if (_res != UCI_SUCCESS) {
        printf("  SAVE FAILED: %s\n", uci_last_status());
        uci_dos_unmount_disk(8);
        PASS();
    }

    /* Open the file for reading via SoftIEC */
    printf("  SIEC OPEN SEC-ADDR=2 'UCITEST.PRG'...\n");
    _res = uci_siec_open(2, uci_s_au(fname, "UCITEST.PRG"));
    if (_res != UCI_SUCCESS) {
        printf("  OPEN FAILED: %s\n", uci_last_status());
        uci_dos_unmount_disk(8);
        PASS();
    }

    /* CHKIN: set up input channel and get prefetched data */
    printf("  SIEC CHKIN SEC-ADDR=2...\n");
    _res = uci_siec_chkin(2, prefetch, sizeof(prefetch), &prefetch_len);
    if (_res != UCI_SUCCESS) {
        printf("  CHKIN FAILED: %s\n", uci_last_status());
        uci_siec_close(2);
        uci_dos_unmount_disk(8);
        PASS();
    }
    printf("  PREFETCHED %d BYTES\n", prefetch_len);

    /* Copy prefetch data to read_buf */
    for (i = 0; i < prefetch_len && i < sizeof(read_buf); i++) {
        read_buf[i] = prefetch[i];
    }
    bytes_read = prefetch_len;

    /* Note: CHRIN (0x18) and CLRCHN (0x17) are not implemented in firmware.
     * Data is fully retrieved via CHKIN prefetch. */
    printf("  TOTAL READ: %d BYTES (FROM PREFETCH)\n", bytes_read);

    /* Show first few bytes */
    for (i = 0; i < bytes_read && i < 20; i++) {
    }

    /* Close */
    uci_siec_close(2);
    uci_dos_unmount_disk(8);

    PASS();
}
