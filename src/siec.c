/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * siec.c - Virtual IEC serial bus (SoftIEC).
 *
 * Lets the C64 load programs from and save programs to the
 * Ultimate device's storage the same way it would use a real
 * floppy drive.  Supports LOAD, SAVE, OPEN, CLOSE, CHKIN, and
 * CHKOUT operations as well as a "mount and load" shortcut.
 */

#include "uci.h"
#include "internal.h"
#include <string.h>

/* SoftIEC command IDs */
#define SIEC_CMD_IDENTIFY      0x01
#define SIEC_CMD_LOAD_SU       0x10
#define SIEC_CMD_LOAD_EX       0x11
#define SIEC_CMD_SAVE          0x12
#define SIEC_CMD_OPEN          0x13
#define SIEC_CMD_CLOSE         0x14
#define SIEC_CMD_CHKIN         0x15
#define SIEC_CMD_CHKOUT        0x16
#define SIEC_CMD_LOAD_MOUNT_SU 0x1A
#define SIEC_CMD_LOAD_MOUNT_EX 0x1B

/* Binary status codes from SoftIEC */
#define SIEC_STATUS_OK              0x00
#define SIEC_STATUS_FILE_NOT_FOUND  0x01
#define SIEC_STATUS_SAVE_ERROR      0x02
#define SIEC_STATUS_NO_INPUT_CHAN   0x03
#define SIEC_STATUS_UNKNOWN_CMD     0x04
#define SIEC_STATUS_NO_IEC_MODULE   0x05

/* Map status to UCI error code.
 * The firmware may return either binary status (\x00, \x01, ...)
 * or text status ("00,OK", "82,FILE NOT FOUND", ...) depending on
 * the command.  Handle both formats. */
static uci_err_t siec_status_to_error(void) {
    if (uci_last_status_buf[0] == '\0') return UCI_SUCCESS;

    /* Text status: "NN,..." */
    if (uci_last_status_buf[0] >= '0' && uci_last_status_buf[0] <= '9' &&
        uci_last_status_buf[1] >= '0' && uci_last_status_buf[1] <= '9' &&
        uci_last_status_buf[2] == ',') {
        uint8_t code = (uint8_t)((uci_last_status_buf[0] - '0') * 10 +
                                 (uci_last_status_buf[1] - '0'));
        return (code == 0) ? UCI_SUCCESS : UCI_ERR_STATUS;
    }

    /* Binary status */
    switch ((uint8_t)uci_last_status_buf[0]) {
        case SIEC_STATUS_OK:             return UCI_SUCCESS;
        case SIEC_STATUS_FILE_NOT_FOUND: return UCI_ERR_STATUS;
        case SIEC_STATUS_SAVE_ERROR:     return UCI_ERR_STATUS;
        case SIEC_STATUS_NO_INPUT_CHAN:  return UCI_ERR_STATUS;
        case SIEC_STATUS_UNKNOWN_CMD:    return UCI_ERR_STATUS;
        case SIEC_STATUS_NO_IEC_MODULE:  return UCI_ERR_STATUS;
        default:                         return UCI_ERR_STATUS;
    }
}

uci_err_t uci_siec_identify(char *buf, uint16_t max_len) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_IDENTIFY);
    if (res != UCI_SUCCESS) return res;
    res = uci_execute_cmd((uint8_t *)buf, max_len - 1, &reply_len);
    if (res == UCI_SUCCESS) {
        buf[reply_len] = '\0';
    }
    return res;
}

uci_err_t uci_siec_load_su(uci_sec_addr_t sec_addr, bool verify, uint16_t load_addr,
                          uint16_t end_addr, const char *filename,
                          uint16_t *start_addr) {
    uint8_t reply[2];
    uint16_t reply_len;
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_LOAD_SU);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(verify ? 1 : 0);
    uci_write_cmd_byte(load_addr & 0xFF);
    uci_write_cmd_byte((load_addr >> 8) & 0xFF);
    uci_write_cmd_byte(end_addr & 0xFF);
    uci_write_cmd_byte((end_addr >> 8) & 0xFF);
    uci_write_cstr(filename);

    res = uci_execute_cmd_raw(reply, sizeof(reply), &reply_len);
    if (res != UCI_SUCCESS) return res;
    res = siec_status_to_error();
    if (res != UCI_SUCCESS) return res;
    if (start_addr && reply_len >= 2) {
        *start_addr = (uint16_t)reply[0] | ((uint16_t)reply[1] << 8);
    }
    return UCI_SUCCESS;
}

uci_err_t uci_siec_load_ex(uci_sec_addr_t sec_addr, bool verify, uint16_t *end_addr) {
    uint8_t reply[3];
    uint16_t reply_len;
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_LOAD_EX);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(verify ? 1 : 0);

    res = uci_execute_cmd_raw(reply, sizeof(reply), &reply_len);
    if (res != UCI_SUCCESS) return res;
    res = siec_status_to_error();
    if (res != UCI_SUCCESS) return res;
    if (!verify && end_addr && reply_len >= 3) {
        *end_addr = (uint16_t)reply[1] | ((uint16_t)reply[2] << 8);
    }
    return UCI_SUCCESS;
}

/* Save a memory block via the SoftIEC interface.
 *
 * NOTE: SoftIEC commands operate directly on the host FAT partition (e.g. /usb0/)
 * rather than mounted D64 images on drive 8, unless the SoftIEC path has been
 * explicitly CD'd into the D64 image using the command channel (secondary address 15)
 * by sending "CD:<path_to_d64>".
 *
 * If the target file already exists on the filesystem/disk image, the save command
 * will fail with SIEC_STATUS_SAVE_ERROR (0x02). To overwrite an existing file,
 * prefix the filename with '@' (e.g. "@UCITEST"), which is the standard Commodore
 * DOS replace syntax. The firmware translates this prefix into FA_CREATE_ALWAYS
 * write flags to successfully overwrite the file.
 */
uci_err_t uci_siec_save(bool verify, uci_sec_addr_t sec_addr,
                       uint16_t start_addr, uint16_t end_addr,
                       const char *filename) {
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_SAVE);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(verify ? 1 : 0);
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(start_addr & 0xFF);
    uci_write_cmd_byte((start_addr >> 8) & 0xFF);
    uci_write_cmd_byte(end_addr & 0xFF);
    uci_write_cmd_byte((end_addr >> 8) & 0xFF);
    uci_write_cstr(filename);

    res = uci_execute_cmd_raw(NULL, 0, NULL);
    if (res != UCI_SUCCESS) return res;
    return siec_status_to_error();
}

uci_err_t uci_siec_open(uci_sec_addr_t sec_addr, const char *filename) {
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_OPEN);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(0x00); /* unused */
    uci_write_cstr(filename);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_siec_close(uci_sec_addr_t sec_addr) {
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_CLOSE);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(0x00); /* unused */
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_siec_chkin(uci_sec_addr_t sec_addr, uint8_t *prefetch,
                        uint16_t prefetch_max, uint16_t *prefetch_len) {
    uint16_t reply_len;
    uci_err_t res;

    if (prefetch_len) *prefetch_len = 0;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_CHKIN);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(0x00); /* unused */

    res = uci_execute_cmd_raw(prefetch, prefetch_max, &reply_len);
    if (res != UCI_SUCCESS) return res;
    res = siec_status_to_error();
    if (res != UCI_SUCCESS) return res;
    if (prefetch_len) *prefetch_len = reply_len;
    return UCI_SUCCESS;
}

uci_err_t uci_siec_chkout(uci_sec_addr_t sec_addr, const uint8_t *data, uint16_t len) {
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_CHKOUT);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(0x00); /* unused */
    if (data && len > 0) {
        uci_write_cmd_bytes(data, len);
    }
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_siec_load_mount_su(uci_sec_addr_t sec_addr, bool verify, uint16_t load_addr,
                                 uint16_t end_addr, const char *filename,
                                 uint16_t *start_addr) {
    uint8_t reply[2];
    uint16_t reply_len;
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_LOAD_MOUNT_SU);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(verify ? 1 : 0);
    uci_write_cmd_byte(load_addr & 0xFF);
    uci_write_cmd_byte((load_addr >> 8) & 0xFF);
    uci_write_cmd_byte(end_addr & 0xFF);
    uci_write_cmd_byte((end_addr >> 8) & 0xFF);
    uci_write_cstr(filename);

    res = uci_execute_cmd_raw(reply, sizeof(reply), &reply_len);
    if (res != UCI_SUCCESS) return res;
    res = siec_status_to_error();
    if (res != UCI_SUCCESS) return res;
    if (start_addr && reply_len >= 2) {
        *start_addr = (uint16_t)reply[0] | ((uint16_t)reply[1] << 8);
    }
    return UCI_SUCCESS;
}

uci_err_t uci_siec_load_mount_ex(uci_sec_addr_t sec_addr, bool verify, uint16_t *end_addr) {
    uint8_t reply[3];
    uint16_t reply_len;
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_SOFTIEC, SIEC_CMD_LOAD_MOUNT_EX);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)sec_addr);
    uci_write_cmd_byte(verify ? 1 : 0);

    res = uci_execute_cmd_raw(reply, sizeof(reply), &reply_len);
    if (res != UCI_SUCCESS) return res;
    res = siec_status_to_error();
    if (res != UCI_SUCCESS) return res;
    if (!verify && end_addr && reply_len >= 3) {
        *end_addr = (uint16_t)reply[1] | ((uint16_t)reply[2] << 8);
    }
    return UCI_SUCCESS;
}

/* High-level SoftIEC Directory Listing */

static uint8_t siec_dir_buf[512];
static uint16_t siec_dir_len;
static uint16_t siec_dir_ofs;

uci_err_t uci_siec_open_dir(uci_drive_t drive_id) {
    uci_err_t res;
    (void)drive_id;

    res = uci_siec_open(UCI_SEC_DIR, "$");
    if (res != UCI_SUCCESS) return res;

    res = uci_siec_chkin(UCI_SEC_DIR, siec_dir_buf, sizeof(siec_dir_buf), &siec_dir_len);
    if (res != UCI_SUCCESS) {
        uci_siec_close(UCI_SEC_DIR);
        return res;
    }

    siec_dir_ofs = 32;
    return UCI_SUCCESS;
}

uci_err_t uci_siec_read_dir(uci_dir_entry_t *entry) {
    uint8_t i, j;
    if (!entry) return UCI_ERR_PARAM;

    while (siec_dir_ofs + 31 < siec_dir_len) {
        uint16_t cur_ofs = siec_dir_ofs;
        siec_dir_ofs += 32;

        if (siec_dir_buf[cur_ofs] == 0x00 && siec_dir_buf[cur_ofs + 1] == 0x00) {
            continue;
        }

        entry->blocks = (uint16_t)siec_dir_buf[cur_ofs] | ((uint16_t)siec_dir_buf[cur_ofs + 1] << 8);

        for (i = 0; i < 16 && (cur_ofs + 2 + i < siec_dir_len); i++) {
            uint8_t b = siec_dir_buf[cur_ofs + 2 + i];
            if (b == 0xA0) break;
            entry->filename[i] = (char)b;
        }
        entry->filename[i] = '\0';

        for (j = 0; j < 3 && (cur_ofs + 18 + j < siec_dir_len); j++) {
            entry->type[j] = (char)siec_dir_buf[cur_ofs + 18 + j];
        }
        entry->type[j] = '\0';

        entry->is_dir = (entry->type[0] == 'D' && entry->type[1] == 'I' && entry->type[2] == 'R');

        return UCI_SUCCESS;
    }

    return UCI_ERR_END_OF_LISTING;
}

uci_err_t uci_siec_close_dir(void) {
    return uci_siec_close(UCI_SEC_DIR);
}
