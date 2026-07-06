/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

int main(void) {

    TEST("CORE INIT");

    /* uci_init() already called in TEST_SETUP and checked for success.
     * Verify base address is non-zero. */
    {
        uint16_t base = uci_get_base();
        printf("  BASE: $%04X\n", base);
        if (base == 0) {
            FAIL("WRONG_BASE");
        }
    }

    PASS();
}
