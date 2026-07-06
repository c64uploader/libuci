/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static const char *status;
static uint8_t code;

int main(void) {
    uint8_t _res;

    TEST("CORE ERROR STATE");

    /* Deliberately trigger a failing command: open a non-existent file for read */
    printf("  OPEN BAD FILE...\n");
    _res = uci_dos_open(UCI_FA_READ, "NONEXISTENT_FILE_12345.TXT");
    printf("  OPEN: RES=%d\n", _res);

    /* The command should fail (either returned error or set error flag) */
    if (!uci_has_error()) {
        FAIL("UNEXPECTED_ERRFLAG");
    }
    printf("  ERR FLAG: SET\n");

    /* Status should contain an error string */
    status = uci_last_status();
    printf("  STATUS: %s\n", status);
    if (status[0] == '\0') {
        FAIL("UNEXPECTED_STATUS");
    }

    /* Status code should be non-zero */
    code = uci_last_status_code();
    printf("  CODE: %d\n", code);
    if (code == 0) {
        FAIL("WRONG_STATUS_CODE");
    }

    /* uci_last_status_ok() should be false */
    if (uci_last_status_ok()) {
        FAIL("UNEXPECTED_OK");
    }

    /* Clean up */
    uci_reset();

    PASS();
}
