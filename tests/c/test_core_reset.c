/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

static const char *status;

int main(void) {

    TEST("CORE RESET");

    /* Trigger a reset */
    printf("  RESET...\n");
    uci_reset();

    /* After reset, error flag should be clear */
    if (uci_has_error()) {
        FAIL("UNEXPECTED_ERRFLAG");
    }
    printf("  ERR FLAG: CLEAR\n");

    /* After reset, status should be empty */
    status = uci_last_status();
    printf("  STATUS: '%s'\n", status);
    if (status[0] != '\0') {
        FAIL("UNEXPECTED_STATUS");
    }

    PASS();
}
