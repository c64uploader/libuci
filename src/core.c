/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * core.c - Communication layer for Ultimate Command Interface library.
 *
 * This file handles detecting the Ultimate hardware, initializing
 * the connection, sending commands to the firmware, and receiving
 * back responses.  All other files in this library build on top
 * of the functions defined here.
 */

#include "internal.h"
#include <stddef.h>

/* Global state */

uint16_t uci_base;

bool     uci_error_flag;
char     uci_last_status_buf[64];
bool     uci_dir_stream_active;

/* Helpers */

static bool probe_uci_base(uint16_t base) {
    volatile uint8_t *cmd_reg = (volatile uint8_t *)(base + 1);
    uint8_t val = *cmd_reg;
    return (val == 0xC9 || val == 0x49);
}

static uint8_t uci_parse_text_status_code(const char *status) {
    if (!status) return 255;
    if (status[0] < '0' || status[0] > '9') return 255;
    if (status[1] < '0' || status[1] > '9') return 255;
    if (status[2] != ',') return 255;
    return (uint8_t)((status[0] - '0') * 10 + (status[1] - '0'));
}

/* Core API */

uci_err_t uci_init(void) {
    uci_error_flag = false;
    uci_last_status_buf[0] = '\0';
    uci_dir_stream_active = false;

    if (probe_uci_base(0xDF1C)) {
        uci_base = 0xDF1C;
    } else if (probe_uci_base(0xDE1C)) {
        uci_base = 0xDE1C;
    } else if (probe_uci_base(0xDFFC)) {
        uci_base = 0xDFFC;
    } else {
        uci_base = 0;
        return UCI_ERR_NO_INTERFACE;
    }

    uci_reset();
    return UCI_SUCCESS;
}

uint16_t uci_get_base(void) {
    return uci_base;
}

void uci_reset(void) {
    uint16_t poll_count;
    uint8_t status;
    if (uci_base == 0) return;

    *(volatile uint8_t *)(uci_base + 0) = CTL_ABORT;

    poll_count = 0;
    do {
        status = *(volatile uint8_t *)(uci_base + 0);
        if (status == 0xFF) break;
        poll_count++;
        if (poll_count > UCI_POLL_LIMIT) break;
        if ((status & STAT_STATE_BITS) != STAT_STATE_IDLE) {
            volatile uint16_t delay_c;
            for (delay_c = 0; delay_c < 250; delay_c++);
        }
    } while ((status & STAT_STATE_BITS) != STAT_STATE_IDLE);

    if (status & STAT_ERROR) {
        *(volatile uint8_t *)(uci_base + 0) = CTL_CLR_ERR;
    }

    uci_error_flag = false;
    uci_last_status_buf[0] = '\0';
    uci_dir_stream_active = false;
}

bool uci_has_error(void) {
    return uci_error_flag;
}

const char *uci_last_status(void) {
    return uci_last_status_buf;
}

uint8_t uci_last_status_code(void) {
    return uci_parse_text_status_code(uci_last_status_buf);
}

bool uci_last_status_ok(void) {
    uint8_t code = uci_last_status_code();
    return (code == 0);
}

/* Low-level handshake */

uci_err_t uci_start_cmd(uci_target_id_t target, uint8_t cmd) {
    uint8_t status;
    uint16_t poll_count = 0;

    if (uci_base == 0) return UCI_ERR_NO_INTERFACE;

    if (uci_dir_stream_active) {
        uci_reset();
    }

    status = *(volatile uint8_t *)(uci_base + 0);
    if (status == 0xFF) return UCI_ERR_LATCH;

    do {
        status = *(volatile uint8_t *)(uci_base + 0);
        if (status == 0xFF) return UCI_ERR_LATCH;
        poll_count++;
        if (poll_count > UCI_POLL_LIMIT) return UCI_ERR_TIMEOUT;
        if ((status & STAT_STATE_BITS) != STAT_STATE_IDLE) {
            volatile uint16_t delay_c;
            for (delay_c = 0; delay_c < 250; delay_c++);
        }
    } while ((status & STAT_STATE_BITS) != STAT_STATE_IDLE);

    uci_error_flag = false;
    uci_last_status_buf[0] = '\0';

    *(volatile uint8_t *)(uci_base + 1) = (uint8_t)target;
    *(volatile uint8_t *)(uci_base + 1) = cmd;

    return UCI_SUCCESS;
}

void uci_write_cmd_byte(uint8_t byte) {
    *(volatile uint8_t *)(uci_base + 1) = byte;
}

void uci_write_cmd_bytes(const uint8_t *buf, uint16_t len) {
    uint16_t i;
    for (i = 0; i < len; i++) {
        *(volatile uint8_t *)(uci_base + 1) = buf[i];
    }
}

void uci_write_cstr(const char *str) {
    while (*str) {
        *(volatile uint8_t *)(uci_base + 1) = (uint8_t)*str++;
    }
    *(volatile uint8_t *)(uci_base + 1) = 0;
}

/* Common protocol engine.  Scatters reply bytes into iovs[0..iov_count-1];
 * each iovec receives up to its cap bytes (buf==NULL discards).  Bytes
 * beyond the last iovec are consumed and discarded.  total_read reports
 * the full number of bytes consumed from the firmware. */
static uci_err_t uci_execute_cmd_v_common(uci_iov_t *iovs, uint8_t iov_count,
                                          uint16_t *total_read, bool check_text_status) {
    uint8_t status;
    uint8_t state;
    uint16_t poll_count;
    uint8_t cur_iov = 0;
    uint16_t iov_off = 0;
    uint16_t got = 0;

    if (total_read) *total_read = 0;

    *(volatile uint8_t *)(uci_base + 0) = CTL_PUSH_CMD;

    while (1) {
        poll_count = 0;
        do {
            volatile uint16_t delay_c;
            status = *(volatile uint8_t *)(uci_base + 0);
            if (status == 0xFF) return UCI_ERR_LATCH;
            poll_count++;
            if (poll_count > UCI_POLL_LIMIT) return UCI_ERR_TIMEOUT;
            for (delay_c = 0; delay_c < 250; delay_c++);
        } while ((status & STAT_STATE_BITS) == STAT_STATE_BUSY);

        if (status & STAT_ERROR) {
            uci_error_flag = true;
        }

        if (status & STAT_STAT_AV) {
            uint16_t s_idx = 0;
            while (*(volatile uint8_t *)(uci_base + 0) & STAT_STAT_AV) {
                uint8_t s_byte = *(volatile uint8_t *)(uci_base + 3);
                if (s_idx < sizeof(uci_last_status_buf) - 1) {
                    uci_last_status_buf[s_idx++] = (char)s_byte;
                }
            }
            uci_last_status_buf[s_idx] = '\0';
        }

        state = status & STAT_STATE_BITS;

        if (state == STAT_STATE_MORE || state == STAT_STATE_LAST) {
            while (*(volatile uint8_t *)(uci_base + 0) & STAT_DATA_AV) {
                uint8_t r_byte = *(volatile uint8_t *)(uci_base + 2);
                /* Advance past full iovecs */
                while (cur_iov < iov_count && iov_off >= iovs[cur_iov].cap) {
                    cur_iov++;
                    iov_off = 0;
                }
                if (cur_iov < iov_count) {
                    if (iovs[cur_iov].buf)
                        iovs[cur_iov].buf[iov_off] = r_byte;
                    /* else: NULL-buf iov, discard (already read from volatile register) */
                    iov_off++;
                }
                /* else: iovecs exhausted, byte already drained by volatile read above */
                got++;
            }
            *(volatile uint8_t *)(uci_base + 0) = CTL_DATA_ACC;
            if (state == STAT_STATE_LAST) {
                break;
            }
        } else {
            break;
        }
    }

    if (total_read) *total_read = got;

    if (uci_error_flag) {
        return UCI_ERR_HARDWARE;
    }

    if (check_text_status) {
        if (uci_last_status_buf[0] != '\0' &&
            !(uci_last_status_buf[0] == '0' && uci_last_status_buf[1] == '0' && uci_last_status_buf[2] == ',')) {
            uci_error_flag = true;
            return UCI_ERR_STATUS;
        }
    }

    return UCI_SUCCESS;
}

uci_err_t uci_execute_cmd(uint8_t *reply_buf, uint16_t reply_max, uint16_t *reply_len) {
    uci_iov_t iov;
    uint16_t total;
    uci_err_t res;
    iov.buf = reply_buf;  iov.cap = reply_max;
    res = uci_execute_cmd_v_common(&iov, 1, &total, true);
    if (reply_len) {
        uint16_t n = (total <= reply_max) ? total : reply_max;
        *reply_len = n;
    }
    return res;
}

uci_err_t uci_execute_cmd_raw(uint8_t *reply_buf, uint16_t reply_max, uint16_t *reply_len) {
    uci_iov_t iov;
    uint16_t total;
    uci_err_t res;
    iov.buf = reply_buf;  iov.cap = reply_max;
    res = uci_execute_cmd_v_common(&iov, 1, &total, false);
    if (reply_len) {
        uint16_t n = (total <= reply_max) ? total : reply_max;
        *reply_len = n;
    }
    return res;
}

uci_err_t uci_execute_cmd_v(uci_iov_t *iovs, uint8_t iov_count, uint16_t *total_read) {
    return uci_execute_cmd_v_common(iovs, iov_count, total_read, true);
}

uci_err_t uci_execute_cmd_v_raw(uci_iov_t *iovs, uint8_t iov_count, uint16_t *total_read) {
    return uci_execute_cmd_v_common(iovs, iov_count, total_read, false);
}
