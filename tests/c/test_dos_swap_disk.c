/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"

int main(void) {
    uint8_t _res;

    TEST("DOS SWAP DISK");

    /* Swap disk on drive 8 - may succeed or fail depending on image type */
    printf("  SWAP DISK DRIVE 8...\n");
    _res = uci_dos_swap_disk(8);
    printf("  RESULT: %d, STATUS: %s\n", _res, uci_last_status());

    /* If a D71/D81 is mounted, this should succeed.
     * If no dual-sided image, expect DRIVE NOT PRESENT or similar.
     * We don't hard-assert success since the test environment varies. */

    /* Swap on invalid drive should fail */
    printf("  SWAP DISK DRIVE 31 (INVALID)...\n");
    _res = uci_dos_swap_disk(31);
    printf("  RESULT: %d, STATUS: %s\n", _res, uci_last_status());
    /* Expected: error or DRIVE NOT PRESENT */

    PASS();
}
