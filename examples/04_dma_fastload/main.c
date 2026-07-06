/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/04_dma_fastload/main.c - DMA Fast Loader
 *
 * Concept Primer:
 *   Standard 1541 floppy loading over IEC serial bus transfers data at ~400 bytes/sec.
 *   Ultimate SoftIEC DMA fastloading transfers data directly from Ultimate hardware into
 *   C64 RAM while halting the CPU, achieving near-instantaneous transfers (~100 KB/s).
 *   The two-phase process uses uci_siec_load_su (setup: reads 2-byte PRG header to determine
 *   target memory address) followed by uci_siec_load_ex (execute: performs the DMA copy into RAM).
 *
 * Demonstrates:
 *   - SoftIEC DMA load (load_su + load_ex)
 *   - Direct memory access to loaded data
 *   - Hex dump of memory contents
 */

#include <stdio.h>
#include <stdint.h>
#include "uci.h"

/* File to load - must exist on the Ultimate's storage */
#define FILENAME "/temp/example.prg"

/* Hex dump: show 8 rows of 16 bytes each */
static void hexdump(uint16_t addr, uint16_t len) {
    uint16_t i, j;
    uint8_t b;

    for (i = 0; i < len; i += 16) {
        /* Address */
        printf("%04X: ", addr + i);

        /* Hex bytes */
        for (j = 0; j < 16; j++) {
            if (i + j < len) {
                b = *(volatile uint8_t *)(addr + i + j);
                printf("%02X ", b);
            } else {
                printf("   ");
            }
        }

        /* ASCII representation */
        printf(" ");
        for (j = 0; j < 16 && i + j < len; j++) {
            b = *(volatile uint8_t *)(addr + i + j);
            printf("%c", (b >= 0x20 && b < 0x7F) ? b : '.');
        }
        printf("\n");
    }
}

int main(void) {
    uint8_t res;
    uint16_t start_addr = 0;
    uint16_t end_addr = 0;
    uint16_t load_len;

    printf("*** DMA FAST LOADER ***\n\n");

    if (uci_init() != UCI_SUCCESS) {
        printf("UCI HARDWARE NOT FOUND!\n");
        return 1;
    }

    /* Step 1: Prepare the DMA load request.
     * UCI_SEC_LOAD (0) means standard LOAD (not VERIFY).
     * load_addr=0 means "use the 2-byte PRG header address". */
    printf("LOADING: %s\n", FILENAME);
    res = uci_siec_load_su(UCI_SEC_LOAD, false, 0, 0, FILENAME, &start_addr);
    if (res != UCI_SUCCESS) {
        printf("SETUP FAILED: %s\n", uci_last_status());
        return 1;
    }

    printf("TARGET ADDR: $%04X\n\n", start_addr);

    /* Step 2: Execute the DMA transfer.
     * The Ultimate hardware copies the file directly into C64 RAM
     * at high speed - no serial bus bottleneck. */
    res = uci_siec_load_ex(UCI_SEC_LOAD, false, &end_addr);
    if (res != UCI_SUCCESS) {
        printf("DMA FAILED: %s\n", uci_last_status());
        return 1;
    }

    load_len = end_addr - start_addr;
    printf("LOADED %u BYTES ($%04X-$%04X)\n\n",
           load_len, start_addr, end_addr - 1);

    /* Show first 128 bytes of loaded data */
    printf("MEMORY CONTENTS:\n\n");
    hexdump(start_addr, load_len < 128 ? load_len : 128);

    printf("\nDMA TRANSFER COMPLETE!\n");
    return 0;
}
