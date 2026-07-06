/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#include "harness.h"
#include "uci_codec.h"

static const uint8_t data[] = "DELETE ME";

int main(void) {
    uint8_t _res;

    char path[16], fname[20];

    TEST("DOS DELETE");

    /* Ensure we are in a writable directory */

    printf("  CHDIR /TEMP...\n");
    uci_dos_change_dir(uci_s_au(path, "/TEMP"));

    /* Create a file */
    printf("  CREATE FILE...\n");
    _res = uci_dos_open(UCI_FA_WRITE | UCI_FA_CREATE_ALWAYS, uci_s_au(fname, "UCITEST_DL.DAT"));
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_write(data, sizeof(data) - 1);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    _res = uci_dos_close();
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");

    /* Delete it */
    printf("  DELETE...\n");
    _res = uci_dos_delete(uci_s_au(fname, "UCITEST_DL.DAT"));
    printf("  DELETE: RES=%d\n", _res);
    CHECK((_res) == (UCI_SUCCESS), "GOT %d EXPECTED %d");
    CHECK(uci_last_status_ok(), "STATUS: %s");

    /* Verify it's gone by trying to open for read - should fail */
    printf("  VERIFY GONE...\n");
    _res = uci_dos_open(UCI_FA_READ, uci_s_au(fname, "UCITEST_DL.DAT"));
    if (_res == UCI_SUCCESS) {
        /* File still exists - unexpected */
        uci_dos_close();
        FAIL("UNEXPECTED_OK");
    }

    /* Error flag should be set */
    CHECK(uci_has_error(), "EXPECTED ERROR FLAG");

    /* Status should indicate file not found (non-zero code) */
    if (uci_last_status_code() == 0) {
        FAIL("WRONG_STATUS_CODE");
    }

    /* Clean up */
    uci_reset();

    PASS();
}
