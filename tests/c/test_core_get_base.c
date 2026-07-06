/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

int main(void) {

    TEST("CORE GET BASE");

    /* Verify base address is one of the known valid values */
    {
        uint16_t base = uci_get_base();
        printf("  BASE: $%04X\n", base);

        if (base != 0xDF1C && base != 0xDE1C && base != 0xDFFC) {
            FAIL("WRONG_BASE");
        }
    }

    PASS();
}
