/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 */

#ifndef UCI_INTERNAL_H
#define UCI_INTERNAL_H

/*
 * internal.h - Shared declarations for the library internals.
 *
 * This header is included by every .c file in the library.  It
 * declares the global state variables, the hardware register
 * bitmasks, and the low-level command handshake functions that
 * the protocol-specific files (dos.c, ctrl.c, net.c, siec.c)
 * call to talk to the Ultimate device.
 */

#include "uci.h"

/* Shared state - defined in core.c, used by all target files */

extern uint16_t uci_base;
extern bool     uci_error_flag;
extern char     uci_last_status_buf[64];

/* Set by DOS read_dir(), checked by core's uci_start_cmd() to auto-reset
 * a stale directory stream before the next command. */
extern bool     uci_dir_stream_active;

/* Register bitmasks (used by core.c + dos.c for multi-chunk dir reads) */

#define CTL_PUSH_CMD    0x01
#define CTL_DATA_ACC    0x02
#define CTL_ABORT       0x04
#define CTL_CLR_ERR     0x08

#define STAT_ERROR      0x08
#define STAT_STATE_BITS 0x30
#define STAT_STATE_IDLE 0x00
#define STAT_STATE_BUSY 0x10
#define STAT_STATE_LAST 0x20
#define STAT_STATE_MORE 0x30
#define STAT_STAT_AV    0x40
#define STAT_DATA_AV    0x80

#define UCI_POLL_LIMIT  60000U

/* Low-level handshake - implemented in core.c */

uci_err_t uci_start_cmd(uci_target_id_t target, uint8_t cmd);
void      uci_write_cmd_byte(uint8_t byte);
void      uci_write_cmd_bytes(const uint8_t *buf, uint16_t len);
void      uci_write_cstr(const char *str);

/* Scatter/gather reply routing */

/* One destination for reply bytes.  Set buf to NULL to discard those
 * bytes (consume from the firmware but don't store them). */
typedef struct {
    uint8_t *buf;
    uint16_t cap;
} uci_iov_t;

/* Standard execute: parses text status ("00,OK" etc.), returns
 * UCI_ERR_STATUS on any non-success status string. */
uci_err_t uci_execute_cmd(uint8_t *reply_buf, uint16_t reply_max, uint16_t *reply_len);

/* Raw execute: identical protocol but skips the text-status check.
 * For targets that return binary status codes (SoftIEC).  The caller
 * must inspect uci_last_status_buf[0] manually.  Still sets
 * uci_error_flag on hardware errors and still populates
 * uci_last_status_buf. */
uci_err_t uci_execute_cmd_raw(uint8_t *reply_buf, uint16_t reply_max, uint16_t *reply_len);

/* Vector execute: scatter reply bytes into iovs[0..iov_count-1].
 * Each iovec receives up to its cap bytes (buf==NULL discards).  Bytes
 * beyond the last iovec are consumed and discarded.  total_read reports
 * the full number of bytes consumed from the firmware (may be NULL).
 * Lets callers peel off firmware-internal headers or read directly into
 * separate destinations without a temp buffer or memmove. */
uci_err_t uci_execute_cmd_v(uci_iov_t *iovs, uint8_t iov_count, uint16_t *total_read);

/* Like uci_execute_cmd_v but skips the text-status check (SoftIEC). */
uci_err_t uci_execute_cmd_v_raw(uci_iov_t *iovs, uint8_t iov_count, uint16_t *total_read);

#endif /* UCI_INTERNAL_H */
