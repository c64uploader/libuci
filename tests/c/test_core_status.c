/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static uint8_t code;

/* Ultimate returns status as plain ASCII (0x41-0x5A = 'A'-'Z'). */
static const uint8_t kPathDoesNotExist[] = {
    /*P    A    T    H         D    O    E    S    N    '    T         E    X    I    S    T   \0 */
    0x50,0x41,0x54,0x48,0x20,0x44,0x4F,0x45,0x53,0x4E,0x27,0x54,0x20,0x45,0x58,0x49,0x53,0x54,0x00
};

int main(void) {
    uint8_t _res;

    TEST("CORE STATUS");

    /* Reset and do a known-good operation */
    uci_reset();

    /* After reset, status is empty - code should be 255 (not text formatted) */
    printf("  AFTER RESET...\n");
    code = uci_last_status_code();
    printf("  CODE: %d (EXPECT 255)\n", code);
    if (code != 255) {
        FAIL("WRONG_STATUS_CODE");
    }

    /* uci_last_status_ok() should return false for non-00 status */
    if (uci_last_status_ok()) {
        FAIL("UNEXPECTED_OK");
    }

    /* Now trigger a command that produces a "00,..." success status */
    printf("  CHDIR /...\n");
    _res = uci_dos_change_dir("/");
    if (_res == UCI_SUCCESS) {
        code = uci_last_status_code();
        printf("  CODE: %d (EXPECT 0)\n", code);
        if (code != 0) {
            FAIL("WRONG_STATUS_CODE");
        }
        if (!uci_last_status_ok()) {
            FAIL("UNEXPECTED_OK");
        }
    }

    /* Now trigger an error to get a non-zero status code */
    printf("  CLOSE (BAD)...\n");
    _res = uci_dos_close();
    code = uci_last_status_code();
    printf("  CODE: %d\n", code);
    if (code == 0 || code == 255) {
        FAIL("WRONG_STATUS_CODE");
    }

    /* Now trigger a non-standard error string (no prefix) and assert it yields code 255 */
    printf("  OPEN NONEXISTENT...\n");
    _res = uci_dos_open(UCI_FA_READ, "NONEXISTENT_STATUS_TEST_XYZ.TXT");
    code = uci_last_status_code();
    printf("  CODE: %d (EXPECT 255)\n", code);
    CHECK((code) == (255), "GOT %d EXPECTED %d");
    CHECK(!uci_last_status_ok(), "STATUS UNEXPECTEDLY OK");
    {
        const char *status_str = uci_last_status();
        printf("  STATUS: %s\n", status_str);
        CHECK(memcmp(status_str, kPathDoesNotExist, sizeof(kPathDoesNotExist)) == 0, "EXPECTED TRUE");
    }

    /* Clean up */
    uci_reset();

    PASS();
}
