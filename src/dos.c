/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * dos.c - File operations on the Ultimate device's storage (USB or SD card).
 *
 * You can open, read, write, delete, rename, and copy files and
 * directories.  You can also mount and unmount disk images (D64),
 * navigate the directory tree, and get or set the real-time clock.
 */

#include "uci.h"
#include "internal.h"

/* DOS command IDs - only used inside this file */
#define DOS_CMD_IDENTIFY       0x01
#define DOS_CMD_OPEN_FILE      0x02
#define DOS_CMD_CLOSE_FILE     0x03
#define DOS_CMD_READ_DATA      0x04
#define DOS_CMD_WRITE_DATA     0x05
#define DOS_CMD_FILE_SEEK      0x06
#define DOS_CMD_DELETE_FILE    0x09
#define DOS_CMD_RENAME_FILE    0x0A
#define DOS_CMD_COPY_FILE      0x0B
#define DOS_CMD_CHANGE_DIR     0x11
#define DOS_CMD_GET_PATH       0x12
#define DOS_CMD_OPEN_DIR       0x13
#define DOS_CMD_READ_DIR       0x14
#define DOS_CMD_CREATE_DIR     0x16
#define DOS_CMD_MOUNT_DISK     0x23
#define DOS_CMD_UNMOUNT_DISK   0x24
#define DOS_CMD_FILE_INFO      0x07
#define DOS_CMD_FILE_STAT      0x08
#define DOS_CMD_COPY_HOME_PATH 0x17
#define DOS_CMD_SWAP_DISK      0x25
#define DOS_CMD_GET_TIME       0x26
#define DOS_CMD_SET_TIME       0x27

/* Directory stream state - only used inside this file */
typedef enum { DIR_IDLE, DIR_STREAMING, DIR_DONE_PENDING } dir_state_t;
static dir_state_t dir_state;

uci_err_t uci_dos_identify(char *buf, uint16_t max_len) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_IDENTIFY);
    if (res != UCI_SUCCESS) return res;
    res = uci_execute_cmd((uint8_t *)buf, max_len - 1, &reply_len);
    if (res == UCI_SUCCESS) {
        buf[reply_len] = '\0';
    }
    return res;
}

uci_err_t uci_dos_open(uint8_t mode, const char *filename) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_OPEN_FILE);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(mode);
    uci_write_cstr(filename);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_close(void) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_CLOSE_FILE);
    if (res != UCI_SUCCESS) return res;
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_read(uint8_t *buf, uint16_t len, uint16_t *bytes_read) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_READ_DATA);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(len & 0xFF);
    uci_write_cmd_byte((len >> 8) & 0xFF);
    return uci_execute_cmd(buf, len, bytes_read);
}

uci_err_t uci_dos_write(const uint8_t *buf, uint16_t len) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_WRITE_DATA);
    if (res != UCI_SUCCESS) return res;
    /* Bytes 2-3 of DOS_CMD_WRITE_DATA are unused by the firmware (the
       payload length is implicit from the command packet length).  They
       are sent as zero for protocol symmetry with DOS_CMD_READ_DATA. */
    uci_write_cmd_byte(0x00);
    uci_write_cmd_byte(0x00);
    uci_write_cmd_bytes(buf, len);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_seek(uint32_t offset) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_FILE_SEEK);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(offset & 0xFF);
    uci_write_cmd_byte((offset >> 8) & 0xFF);
    uci_write_cmd_byte((offset >> 16) & 0xFF);
    uci_write_cmd_byte((offset >> 24) & 0xFF);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_delete(const char *filename) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_DELETE_FILE);
    if (res != UCI_SUCCESS) return res;
    uci_write_cstr(filename);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_rename(const char *oldname, const char *newname) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_RENAME_FILE);
    if (res != UCI_SUCCESS) return res;
    uci_write_cstr(oldname);
    uci_write_cstr(newname);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_copy(const char *src, const char *dest) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_COPY_FILE);
    if (res != UCI_SUCCESS) return res;
    uci_write_cstr(src);
    uci_write_cstr(dest);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_change_dir(const char *path) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_CHANGE_DIR);
    if (res != UCI_SUCCESS) return res;
    uci_write_cstr(path);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_get_path(char *buf, uint16_t max_len) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_GET_PATH);
    if (res != UCI_SUCCESS) return res;
    res = uci_execute_cmd((uint8_t *)buf, max_len - 1, &reply_len);
    if (res == UCI_SUCCESS) {
        buf[reply_len] = '\0';
    }
    return res;
}

uci_err_t uci_dos_open_dir(void) {
    uci_err_t res;
    dir_state = DIR_IDLE;
    uci_dir_stream_active = false;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_OPEN_DIR);
    if (res != UCI_SUCCESS) return res;
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_read_dir(uint8_t *attrib, char *filename_buf, uint16_t max_len) {
    uint8_t status;
    uint16_t poll_count;
    uint16_t bytes_read = 0;
    uci_err_t res;
    uint8_t ch;
    uint8_t cur_state;

    if (uci_base == 0) return UCI_ERR_NO_INTERFACE;

    if (dir_state == DIR_DONE_PENDING) {
        dir_state = DIR_IDLE;
        uci_dir_stream_active = false;
        return UCI_ERR_END_OF_LISTING;
    }

    if (dir_state == DIR_IDLE) {
        res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_READ_DIR);
        if (res != UCI_SUCCESS) return res;

        *(volatile uint8_t *)(uci_base + 0) = CTL_PUSH_CMD;
        dir_state = DIR_STREAMING;
        uci_dir_stream_active = true;
    } else {
        *(volatile uint8_t *)(uci_base + 0) = CTL_DATA_ACC;
    }

    poll_count = 0;
    do {
        status = *(volatile uint8_t *)(uci_base + 0);
        if (status == 0xFF) {
            dir_state = DIR_IDLE;
            uci_dir_stream_active = false;
            return UCI_ERR_LATCH;
        }
        poll_count++;
        if (poll_count > UCI_POLL_LIMIT) {
            dir_state = DIR_IDLE;
            uci_dir_stream_active = false;
            return UCI_ERR_TIMEOUT;
        }
        if ((status & STAT_STATE_BITS) == STAT_STATE_BUSY) {
            volatile uint16_t delay_c;
            for (delay_c = 0; delay_c < 250; delay_c++);
        }
    } while ((status & STAT_STATE_BITS) == STAT_STATE_BUSY);

    cur_state = status & STAT_STATE_BITS;

    if (cur_state == STAT_STATE_MORE || cur_state == STAT_STATE_LAST) {
        if (status & STAT_DATA_AV) {
            if (attrib) {
                *attrib = *(volatile uint8_t *)(uci_base + 2);
            } else {
                (void)*(volatile uint8_t *)(uci_base + 2);
            }

            while (*(volatile uint8_t *)(uci_base + 0) & STAT_DATA_AV) {
                ch = *(volatile uint8_t *)(uci_base + 2);
                if (filename_buf && (bytes_read < max_len - 1)) {
                    filename_buf[bytes_read++] = (char)ch;
                }
            }
            if (filename_buf) {
                filename_buf[bytes_read] = '\0';
            }
        }

        if (cur_state == STAT_STATE_LAST) {
            *(volatile uint8_t *)(uci_base + 0) = CTL_DATA_ACC;
            dir_state = DIR_DONE_PENDING;
        }

        return UCI_SUCCESS;
    }

    dir_state = DIR_IDLE;
    uci_dir_stream_active = false;
    return UCI_ERR_HARDWARE;
}

uci_err_t uci_dos_create_dir(const char *dirname) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_CREATE_DIR);
    if (res != UCI_SUCCESS) return res;
    uci_write_cstr(dirname);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_get_time(uci_time_format_t format, char *buf, uint16_t max_len) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_GET_TIME);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)format);
    res = uci_execute_cmd((uint8_t *)buf, max_len - 1, &reply_len);
    if (res == UCI_SUCCESS) {
        buf[reply_len] = '\0';
    }
    return res;
}

uci_err_t uci_dos_mount_disk(uci_drive_t drive_id, const char *filename) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_MOUNT_DISK);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)drive_id);
    uci_write_cstr(filename);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_unmount_disk(uci_drive_t drive_id) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_UNMOUNT_DISK);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)drive_id);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_file_info(uci_file_info_t *info) {
    uci_iov_t iovs[4];
    uint16_t total;
    uint8_t  ext_tmp[3];
    uci_err_t res;
    uint16_t name_len;

    iovs[0].buf = (uint8_t *)info;           iovs[0].cap = 8;  /* size + date + time */
    iovs[1].buf = ext_tmp;                   iovs[1].cap = 3;  /* ext (3 bytes in firmware) */
    iovs[2].buf = &info->attrib;             iovs[2].cap = 1;
    iovs[3].buf = (uint8_t *)info->filename; iovs[3].cap = 31;

    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_FILE_INFO);
    if (res != UCI_SUCCESS) return res;

    res = uci_execute_cmd_v(iovs, 4, &total);
    if (res != UCI_SUCCESS) return res;

    info->ext[0] = ext_tmp[0];
    info->ext[1] = ext_tmp[1];
    info->ext[2] = ext_tmp[2];
    info->ext[3] = '\0';

    name_len = total > 11 ? total - 11 : 0;
    if (name_len > 31) name_len = 31;
    info->filename[name_len] = '\0';

    return UCI_SUCCESS;
}

uci_err_t uci_dos_file_stat(const char *filename, uci_file_info_t *info) {
    uci_iov_t iovs[4];
    uint16_t total;
    uint8_t  ext_tmp[3];
    uci_err_t res;
    uint16_t name_len;

    iovs[0].buf = (uint8_t *)info;           iovs[0].cap = 8;  /* size + date + time */
    iovs[1].buf = ext_tmp;                   iovs[1].cap = 3;  /* ext (3 bytes in firmware) */
    iovs[2].buf = &info->attrib;             iovs[2].cap = 1;
    iovs[3].buf = (uint8_t *)info->filename; iovs[3].cap = 31;

    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_FILE_STAT);
    if (res != UCI_SUCCESS) return res;
    uci_write_cstr(filename);

    res = uci_execute_cmd_v(iovs, 4, &total);
    if (res != UCI_SUCCESS) return res;

    info->ext[0] = ext_tmp[0];
    info->ext[1] = ext_tmp[1];
    info->ext[2] = ext_tmp[2];
    info->ext[3] = '\0';

    name_len = total > 11 ? total - 11 : 0;
    if (name_len > 31) name_len = 31;
    info->filename[name_len] = '\0';

    return UCI_SUCCESS;
}

uci_err_t uci_dos_swap_disk(uci_drive_t drive_id) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_SWAP_DISK);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)drive_id);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_set_time(uint16_t year, uint8_t month, uint8_t day,
                           uint8_t hour, uint8_t min, uint8_t sec) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_SET_TIME);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)(year - 1900));
    uci_write_cmd_byte(month);
    uci_write_cmd_byte(day);
    uci_write_cmd_byte(hour);
    uci_write_cmd_byte(min);
    uci_write_cmd_byte(sec);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_dos_copy_home_path(char *buf, uint16_t max_len) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_DOS, DOS_CMD_COPY_HOME_PATH);
    if (res != UCI_SUCCESS) return res;
    res = uci_execute_cmd((uint8_t *)buf, max_len - 1, &reply_len);
    if (res == UCI_SUCCESS) {
        buf[reply_len] = '\0';
    }
    return res;
}