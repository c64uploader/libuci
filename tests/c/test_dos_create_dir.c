/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static char filename[64];
static uint8_t attrib;
static uint16_t entry_count;
static bool found;
static char *p;
static const char *t;
static bool match;
static char c, tc;

int main(void) {
    uint8_t _res;
    char path[16], fname[16];

    TEST("DOS CREATE DIR");

    /* Change to temp directory */
    printf("  CHDIR /TEMP...\n");
    _res = uci_dos_change_dir(uci_s_au(path, "/TEMP"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Delete ucitestdir in case it exists from a previous run */
    printf("  CLEANUP OLD...\n");
    _res = uci_dos_delete(uci_s_au(fname, "UCITESTDIR"));

    /* Create a new directory */
    printf("  MKDIR...\n");
    _res = uci_dos_create_dir(uci_s_au(fname, "UCITESTDIR"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Change into it */
    printf("  CHDIR UCITESTDIR...\n");
    _res = uci_dos_change_dir(uci_s_au(fname, "UCITESTDIR"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* List it - should be empty or contain only . and .. */
    printf("  LIST DIR...\n");
    _res = uci_dos_open_dir();
    if (_res != UCI_SUCCESS) {
        CHECK((_res) == (UCI_ERR_STATUS), "GOT %d EXPECTED %d");
        CHECK((uci_last_status_code()) == (1), "GOT %d EXPECTED %d");
    } else {
        entry_count = 0;
        while ((_res = uci_dos_read_dir(&attrib, filename, sizeof(filename))) == UCI_SUCCESS) {
            entry_count++;
            if (entry_count > 10) break;
        }
        if (_res != UCI_ERR_END_OF_LISTING) {
            printf("  READ DIR ERROR: %d\n", _res);
        }
        printf("  ENTRIES: %d\n", entry_count);
    }

    /* Go back to temp directory */
    printf("  CHDIR /TEMP...\n");
    _res = uci_dos_change_dir(uci_s_au(path, "/TEMP"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Verify the directory appears in listing */
    printf("  VERIFY IN LISTING...\n");
    _res = uci_dos_open_dir();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    found = false;
    while ((_res = uci_dos_read_dir(&attrib, filename, sizeof(filename))) == UCI_SUCCESS) {
        printf("  ENTRY: '%s'\n", filename);
        /* Case-insensitive compare for "UCITESTDIR" */
        match = true;
        p = filename;
        t = "UCITESTDIR";
        while (*p && *t) {
            c = *p;
            tc = *t;
            if ((uint8_t)c >= 193 && (uint8_t)c <= 218) c -= 128;
            if ((uint8_t)tc >= 193 && (uint8_t)tc <= 218) tc -= 128;
            if (c >= 'a' && c <= 'z') c -= 32;
            if (tc >= 'a' && tc <= 'z') tc -= 32;
            if (c != tc) { match = false; break; }
            p++;
            t++;
        }
        if (match && *p == *t) {
            found = true;
            break;
        }
    }

    if (!found) {
        printf("  DIR NOT FOUND\n");
        FAIL("DIR_UNEXPECTED");
    }

    /* Clean up: try to remove the directory (may fail if non-empty, that's ok) */
    printf("  CLEANUP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));
    uci_dos_delete(uci_s_au(fname, "UCITESTDIR"));

    PASS();
}
