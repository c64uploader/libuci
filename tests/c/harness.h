/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * Minimal test harness for libuci (C89, cc65-compatible).
 *
 * C tests print results via printf. The Go host reads the screen after
 * polling $D7FF for completion. No shared memory protocol.
 *
 * Usage:
 *   #include "harness.h"
 *   int main(void) {
 *       uint8_t res;
 *       TEST("my test");
 *       res = some_uci_call();
 *       CHECK(res == UCI_SUCCESS, "call failed");
 *       PASS();
 *   }
 */
#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "uci.h"

/* Signal register: Go host polls this for non-zero. */
#define SIGNAL  (*(volatile uint8_t *)0xD7FF)

/* Host->C64 input buffer (8 bytes at $C008). Raw bytes written by Go host. */
#define HOST_IN ((volatile uint8_t *)0xC008)

/* test lifecycle */

#define TEST(name) \
    const char *_tn = (name); \
    printf("[%s]\n", _tn); \
    if (uci_init() != 0) { \
        printf("  FAIL uci_init\n"); \
        SIGNAL = 1; return 1; \
    }

#define PASS() \
    printf("  PASS\n"); \
    SIGNAL = 1; return 0;

#define FAIL(msg) \
    printf("  FAIL %s (LINE %d)\n", msg, __LINE__); \
    SIGNAL = 1; return 1;

#define CHECK(cond, msg) \
    if (!(cond)) { \
        printf("  FAIL %s (LINE %d)\n", msg, __LINE__); \
        SIGNAL = 1; return 1; \
    }

#endif
