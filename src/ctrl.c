/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * ctrl.c - Control commands for the Ultimate device.
 *
 * Functions for querying the device identity and hardware
 * information, rebooting the C64, enabling and disabling
 * virtual disk drives, erasing EasyFlash sectors, and reading
 * RAMDisk layout.
 */

#include "uci.h"
#include "internal.h"
#include <string.h>

/* Control command IDs */
#define CTRL_CMD_IDENTIFY      0x01
#define CTRL_CMD_REBOOT        0x06
#define CTRL_CMD_ENABLE_DISK_A 0x30
#define CTRL_CMD_DISABLE_DISK_A 0x31
#define CTRL_CMD_ENABLE_DISK_B 0x32
#define CTRL_CMD_DISABLE_DISK_B 0x33
#define CTRL_CMD_DISK_A_POWER  0x34
#define CTRL_CMD_DISK_B_POWER  0x35
#define CTRL_CMD_GET_HWINFO      0x28
#define CTRL_CMD_GET_DRVINFO     0x29
#define CTRL_CMD_EASYFLASH       0x20
#define CTRL_CMD_GET_RAMDISKINFO 0x40

uci_err_t uci_ctrl_identify(char *buf, uint16_t max_len) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_IDENTIFY);
    if (res != UCI_SUCCESS) return res;
    res = uci_execute_cmd((uint8_t *)buf, max_len - 1, &reply_len);
    if (res == UCI_SUCCESS) {
        buf[reply_len] = '\0';
    }
    return res;
}

uci_err_t uci_ctrl_reboot(void) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_REBOOT);
    if (res != UCI_SUCCESS) return res;
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_ctrl_enable_disk_a(void) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_ENABLE_DISK_A);
    if (res != UCI_SUCCESS) return res;
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_ctrl_disable_disk_a(void) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_DISABLE_DISK_A);
    if (res != UCI_SUCCESS) return res;
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_ctrl_enable_disk_b(void) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_ENABLE_DISK_B);
    if (res != UCI_SUCCESS) return res;
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_ctrl_disable_disk_b(void) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_DISABLE_DISK_B);
    if (res != UCI_SUCCESS) return res;
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_ctrl_disk_a_power(bool *powered_on) {
    char temp[8];
    uint16_t reply_len;
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_DISK_A_POWER);
    if (res != UCI_SUCCESS) return res;

    res = uci_execute_cmd((uint8_t *)temp, sizeof(temp) - 1, &reply_len);
    if (res == UCI_SUCCESS && reply_len >= 2) {
        temp[reply_len] = '\0';
        if (powered_on) {
            *powered_on = (temp[0] == 'o' && temp[1] == 'n');
        }
    }
    return res;
}

uci_err_t uci_ctrl_disk_b_power(bool *powered_on) {
    char temp[8];
    uint16_t reply_len;
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_DISK_B_POWER);
    if (res != UCI_SUCCESS) return res;

    res = uci_execute_cmd((uint8_t *)temp, sizeof(temp) - 1, &reply_len);
    if (res == UCI_SUCCESS && reply_len >= 2) {
        temp[reply_len] = '\0';
        if (powered_on) {
            *powered_on = (temp[0] == 'o' && temp[1] == 'n');
        }
    }
    return res;
}

uci_err_t uci_ctrl_get_hwinfo(uci_hwinfo_dev_t device, char *buf, uint16_t max_len) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_GET_HWINFO);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)device);
    res = uci_execute_cmd((uint8_t *)buf, max_len - 1, &reply_len);
    if (res == UCI_SUCCESS) {
        buf[reply_len] = '\0';
    }
    return res;
}

uci_err_t uci_ctrl_get_sid_info(uci_sid_info_t *info) {
    uint8_t raw[32];
    uci_err_t res;
    uint8_t i, ofs, base;

    if (!info) return UCI_ERR_PARAM;
    memset(info, 0, sizeof(uci_sid_info_t));

    res = uci_ctrl_get_hwinfo(UCI_HWINFO_SID, (char *)raw, sizeof(raw));
    if (res != UCI_SUCCESS) return res;

    info->count = raw[0];
    if (info->count > 4) info->count = 4;

    for (i = 0; i < info->count; i++) {
        ofs = 1 + i * 5;
        base = (raw[ofs] >> 4) | ((raw[ofs + 1] & 0x0F) << 4);
        info->sids[i].base_addr = 0xD000 + (uint16_t)base * 0x10;
        if (raw[ofs + 2] == 1) {
            info->sids[i].model = UCI_SID_MODEL_8580;
        } else if (raw[ofs + 2] == 0) {
            info->sids[i].model = UCI_SID_MODEL_6581;
        } else {
            info->sids[i].model = UCI_SID_MODEL_UNKNOWN;
        }
        info->sids[i].is_hardware = (raw[ofs + 3] != 0);
        info->sids[i].enabled = (raw[ofs + 4] & 0x80) != 0;
    }
    return UCI_SUCCESS;
}

uci_err_t uci_ctrl_get_drvinfo(uci_drive_t effective_id,
                             uci_drive_entry_t *entries, uint8_t max_entries, uint8_t *out_count) {
    uint8_t raw[64];
    uint16_t reply_len;
    uci_err_t res;
    uint8_t count, i, ofs;

    if (out_count) *out_count = 0;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_GET_DRVINFO);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte((uint8_t)effective_id);
    res = uci_execute_cmd(raw, sizeof(raw), &reply_len);
    if (res != UCI_SUCCESS) return res;
    if (reply_len < 1) return UCI_ERR_PARAM;

    count = raw[0];
    if (out_count) *out_count = count;

    if (entries && max_entries > 0) {
        uint8_t to_copy = (count < max_entries) ? count : max_entries;
        for (i = 0; i < to_copy; i++) {
            ofs = 1 + i * 3;
            if (ofs + 2 >= reply_len) break;
            entries[i].type = raw[ofs];
            entries[i].iec_addr = raw[ofs + 1];
            entries[i].power = raw[ofs + 2];
            entries[i].is_drive = (raw[ofs + 2] != 0);
        }
    }
    return UCI_SUCCESS;
}

uci_err_t uci_ctrl_easyflash_erase_sector(uint8_t bank_info, uint8_t base_addr) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_EASYFLASH);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(0x00);   /* subcommand: sector erase */
    uci_write_cmd_byte(bank_info);  /* bits 3-5 select bank (0-7) */
    uci_write_cmd_byte(base_addr);  /* bit 5: 0=low ROM ($8000), 1=high ROM ($A000) */
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_ctrl_get_ramdiskinfo(uci_ramdisk_entry_t entries[4]) {
    uint8_t raw[8];
    uint16_t reply_len;
    uci_err_t res;
    uint8_t i;

    if (!entries) return UCI_ERR_PARAM;
    res = uci_start_cmd(UCI_TARGET_CONTROL, CTRL_CMD_GET_RAMDISKINFO);
    if (res != UCI_SUCCESS) return res;
    res = uci_execute_cmd(raw, sizeof(raw), &reply_len);
    if (res != UCI_SUCCESS) return res;
    if (reply_len < 8) return UCI_ERR_PARAM;

    for (i = 0; i < 4; i++) {
        entries[i].type = raw[i * 2];
        entries[i].size_mb = raw[i * 2 + 1];
    }
    return UCI_SUCCESS;
}