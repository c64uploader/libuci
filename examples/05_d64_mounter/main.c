/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/05_d64_mounter/main.c - Disk Image Explorer
 *
 * Concept Primer:
 *   D64 files are standard 170 KB 1541 floppy disk images containing 35 tracks of CBM IEC sector data.
 *   SoftIEC is the Ultimate device's virtual floppy drive controller. It allows the C64 to interact with
 *   virtual floppy drives using standard CBM KERNAL secondary addresses (0=LOAD, 1=SAVE, 2=Directory, 15=Command).
 *   This example queries available drives, mounts a D64 disk image into drive 8, and reads its directory listing.
 *
 * Demonstrates:
 *   - Drive discovery (uci_ctrl_get_drvinfo)
 *   - Disk image mounting (uci_dos_mount_disk)
 *   - High-level SoftIEC directory listing (uci_siec_open_dir, uci_siec_read_dir, uci_siec_close_dir)
 *   - Combining DOS and SoftIEC targets in one program
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "uci.h"
#include "uci_codec.h"

/* Disk image to mount - must exist on the Ultimate's storage */
#define D64_PATH "/temp/demo.d64"

int main(void) {
    uint8_t res;
    uint8_t drv_count = 0;
    uci_drive_entry_t drives[4];
    uci_dir_entry_t dir_entry;

    printf("*** DISK IMAGE EXPLORER ***\n\n");

    if (uci_init() != UCI_SUCCESS) {
        printf("UCI HARDWARE NOT FOUND!\n");
        return 1;
    }

    /* Step 1: Query available virtual drive units */
    if (uci_ctrl_get_drvinfo(0, drives, 4, &drv_count) == UCI_SUCCESS) {
        printf("VIRTUAL DRIVES DETECTED: %d\n", drv_count);
    }

    /* Step 2: Mount the D64 image to virtual drive 8. */
    printf("MOUNTING: %s\n", D64_PATH);
    res = uci_dos_mount_disk(UCI_DRIVE_8, D64_PATH);
    if (res != UCI_SUCCESS) {
        printf("MOUNT FAILED: %s\n", uci_last_status());
        return 1;
    }
    printf("MOUNTED ON DRIVE 8\n\n");

    /* Step 3: Open and iterate the directory listing via SoftIEC high-level API. */
    res = uci_siec_open_dir(UCI_DRIVE_8);
    if (res != UCI_SUCCESS) {
        printf("DIR OPEN FAILED: %s\n", uci_last_status());
        uci_dos_unmount_disk(UCI_DRIVE_8);
        return 1;
    }

    printf("FILES:\n");
    printf("BLOCKS  TYPE  NAME\n");
    printf("------  ----  ----------------\n");

    while ((res = uci_siec_read_dir(&dir_entry)) == UCI_SUCCESS) {
        static char display_name[32];
        printf("  %4u  %-4s  %s\n", dir_entry.blocks, dir_entry.type, uci_s_d(display_name, dir_entry.filename));
    }

    /* Clean up */
    uci_siec_close_dir();
    uci_dos_unmount_disk(UCI_DRIVE_8);

    printf("\nDISK EXPLORER COMPLETE!\n");
    return 0;
}
