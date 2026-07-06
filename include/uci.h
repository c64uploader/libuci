/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * uci.h - Public header for the Ultimate Command Interface library.
 */

#ifndef UCI_H
#define UCI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Library Error Codes */
typedef enum {
    UCI_SUCCESS           = 0,     /* Success */
    UCI_ERR_NO_INTERFACE   = 1,    /* No Ultimate device found */
    UCI_ERR_TIMEOUT        = 2,    /* Device did not respond in time */
    UCI_ERR_LATCH          = 3,    /* Open bus read ($FF), device may be missing */
    UCI_ERR_HARDWARE       = 4,    /* Device reported a hardware error */
    UCI_ERR_PARAM          = 5,    /* Bad argument passed by caller */
    UCI_ERR_STATUS         = 6,    /* Command finished but returned a non-zero status code */
    UCI_ERR_END_OF_LISTING = 7     /* Directory iteration complete (no more entries) */
} uci_err_t;

/* File open mode flags (for uci_dos_open) */
typedef enum {
    UCI_FA_READ           = 0x01,  /* Read access */
    UCI_FA_WRITE          = 0x02,  /* Write access */
    UCI_FA_CREATE_NEW     = 0x04,  /* Create new file, fail if it already exists */
    UCI_FA_CREATE_ALWAYS  = 0x08,  /* Create new file, overwrite if it exists */
    UCI_FA_OPEN_ALWAYS    = 0x10,  /* Open existing or create new */
    UCI_FA_OPEN_EXISTING  = 0x00,  /* Open existing file, fail if missing (default) */
    UCI_FA_OPEN_FROM_CBM  = 0x80   /* Open a file from a CBM filesystem */
} uci_file_mode_t;

/* Target Subsystem IDs */
typedef enum {
    UCI_TARGET_DOS        = 0x01,
    UCI_TARGET_NETWORK    = 0x03,
    UCI_TARGET_CONTROL    = 0x04,
    UCI_TARGET_SOFTIEC    = 0x05
} uci_target_id_t;

/* Hardware Info Device Selectors */
typedef enum {
    UCI_HWINFO_PRODUCT    = 0,     /* Product string query */
    UCI_HWINFO_SID        = 1      /* SID capabilities query */
} uci_hwinfo_dev_t;

/* DOS Time Formats */
typedef enum {
    UCI_TIME_FORMAT_DATETIME = 0,  /* Date and time string */
    UCI_TIME_FORMAT_WEEKDAY  = 1   /* Weekday date and time string */
} uci_time_format_t;

/* SoftIEC Secondary Addresses & Commands (CBM KERNAL Standard) */
typedef enum {
    UCI_SEC_LOAD          = 0,     /* KERNAL LOAD ($60) */
    UCI_SEC_SAVE          = 1,     /* KERNAL SAVE ($61) */
    UCI_SEC_DIR           = 2,     /* Directory listing ($) */
    UCI_SEC_CMD           = 15     /* Command channel */
} uci_sec_addr_t;

/* Drive Selectors */
typedef enum {
    UCI_DRIVE_DEFAULT     = 0,     /* Default drive */
    UCI_DRIVE_8           = 8,     /* Virtual Drive 8 */
    UCI_DRIVE_9           = 9,     /* Virtual Drive 9 */
    UCI_DRIVE_10          = 10,    /* Virtual Drive 10 */
    UCI_DRIVE_11          = 11     /* Virtual Drive 11 */
} uci_drive_t;

/* Buffer Maximum Sizes */
#define UCI_TIME_STR_MAX        24   /* "WD YYYY/MM/DD HH:MM:SS\0" */
#define UCI_IDENTIFY_STR_MAX    40   /* "ULTIMATE-II NETWORK INTERFACE V1.0" */
#define UCI_PATH_STR_MAX       256   /* Max DOS path length */
#define UCI_HOSTNAME_MAX       128   /* Max DNS hostname length */
#define UCI_FILENAME_MAX        17   /* Standard 16-char filename + '\0' */
#define UCI_DRVINFO_REPLY_MAX   64   /* Max drive info reply size */
#define UCI_RAMDISK_REPLY_LEN    8   /* RAMDisk info reply size */
#define UCI_NAMES_MAX          128   /* Max interface names buffer size */

/* Typed Structures */

/* SID Capabilities */
typedef enum {
    UCI_SID_MODEL_UNKNOWN = 0,
    UCI_SID_MODEL_6581,
    UCI_SID_MODEL_8580
} uci_sid_model_t;

typedef struct {
    uint16_t        base_addr;
    bool            enabled;
    bool            is_hardware; /* true if physical SID socket, false if emulated */
    uci_sid_model_t model;
} uci_sid_entry_t;

typedef struct {
    uint8_t         count;
    uci_sid_entry_t sids[4];
} uci_sid_info_t;

/* Virtual Drive Entry */
typedef struct {
    uint8_t  type;
    uint8_t  iec_addr;
    uint8_t  power;
    bool     is_drive;
} uci_drive_entry_t;

/* RAMDisk Entry */
typedef struct {
    uint8_t type;
    uint8_t size_mb;
} uci_ramdisk_entry_t;

/* SoftIEC Directory Entry */
typedef struct {
    uint16_t blocks;
    char     filename[UCI_FILENAME_MAX]; /* Null-terminated PETSCII string from device. */
    char     type[4];                    /* "PRG", "SEQ", "DEL", etc. */
    bool     is_dir;
} uci_dir_entry_t;

/* Core API */

/* Call once at startup. Detects the device and prepares the library. */
uci_err_t uci_init(void);

/* Returns the memory address used to talk to the device (0 if not found). */
uint16_t uci_get_base(void);

/* Aborts any ongoing command and clears errors. */
void uci_reset(void);

/* True if the last command failed. */
bool uci_has_error(void);

/* Raw status text from the last command (as returned by firmware in ASCII). */
const char *uci_last_status(void);

/* Numeric status code from the status text, e.g. 82. Returns 255 on parse error. */
uint8_t uci_last_status_code(void);

/* True if the status code from the last command is 00. */
bool uci_last_status_ok(void);

/* DOS target types */

typedef struct {
    uint32_t size;
    uint16_t date;          /* DOS date format */
    uint16_t time;          /* DOS time format */
    char     ext[4];        /* File extension as ASCII from firmware */
    uint8_t  attrib;        /* FAT attribute byte */
    char     filename[32];  /* Filename as ASCII from firmware. */
} uci_file_info_t;

/* DOS target functions */

/* Get DOS subsystem version string.
 *
 * buf     - Receives ASCII from firmware.
 * max_len - Size of buf.
 */
uci_err_t uci_dos_identify(char *buf, uint16_t max_len);

/* Open a file on the USB or SD storage.
 *
 * mode     - Bitwise OR of flags:
 *              UCI_FA_READ          Read-only
 *              UCI_FA_WRITE         Write access
 *              UCI_FA_CREATE_NEW    Create new, fail if exists
 *              UCI_FA_CREATE_ALWAYS Create new, overwrite if exists
 *              UCI_FA_OPEN_ALWAYS   Open existing or create new
 *              UCI_FA_OPEN_EXISTING Open existing, fail if missing
 *              UCI_FA_OPEN_FROM_CBM Open from CBM filesystem
 * filename - Uppercase ASCII.
 */
uci_err_t uci_dos_open(uint8_t mode, const char *filename);

/* Close the file that was opened with uci_dos_open. */
uci_err_t uci_dos_close(void);

/* Read up to len bytes from the open file.
 *
 * buf        - Receives raw file data.  No encoding conversion is
 *              performed.  When exchanging text files with other
 *              systems, be aware of their expected encoding.
 * len        - Maximum bytes to read.
 * bytes_read - Receives actual number of bytes read.
 */
uci_err_t uci_dos_read(uint8_t *buf, uint16_t len, uint16_t *bytes_read);

/* Write len bytes to the open file.
 *
 * buf - Raw file data.  No encoding conversion is performed.
 *       When exchanging text files with other systems, be aware
 *       of their expected encoding.
 * len - Number of bytes to write.
 */
uci_err_t uci_dos_write(const uint8_t *buf, uint16_t len);

/* Jump to a byte position in the open file.
 *
 * offset - Byte position from start of file.
 */
uci_err_t uci_dos_seek(uint32_t offset);

/* Delete a file by name.
 *
 * filename - Uppercase ASCII.
 */
uci_err_t uci_dos_delete(const char *filename);

/* Rename a file.
 *
 * oldname - Uppercase ASCII.
 * newname - Uppercase ASCII.
 */
uci_err_t uci_dos_rename(const char *oldname, const char *newname);

/* Copy a file from src to dest.
 *
 * src  - Uppercase ASCII source path.
 * dest - Uppercase ASCII destination path.
 */
uci_err_t uci_dos_copy(const char *src, const char *dest);

/* Change the current directory on the storage.
 *
 * path - Uppercase ASCII.
 */
uci_err_t uci_dos_change_dir(const char *path);

/* Get the full path of the current directory.
 *
 * buf     - Receives ASCII from firmware.
 * max_len - Size of buf.
 */
uci_err_t uci_dos_get_path(char *buf, uint16_t max_len);

/* Start listing a directory.  Call uci_dos_read_dir next to get entries. */
uci_err_t uci_dos_open_dir(void);

/* Read the next entry from the directory listing.
 *
 * Returns UCI_SUCCESS when an entry is available, UCI_ERR_END_OF_LISTING
 * when there are no more entries, or an error code on failure.
 *
 * attrib       - Receives the FAT attribute byte (AM_DIR, AM_RDO, etc.).
 * filename_buf - Receives ASCII from firmware.
 * max_len      - Size of filename_buf.
 */
uci_err_t uci_dos_read_dir(uint8_t *attrib, char *filename_buf, uint16_t max_len);

/* Create a new directory.
 *
 * dirname - Uppercase ASCII.
 */
uci_err_t uci_dos_create_dir(const char *dirname);

/* Read the real-time clock as a formatted string.
 *
 * format - Time format (UCI_TIME_FORMAT_DATETIME or UCI_TIME_FORMAT_WEEKDAY).
 * buf    - Receives ASCII from firmware.
 * max_len - Size of buf.
 */
uci_err_t uci_dos_get_time(uci_time_format_t format, char *buf, uint16_t max_len);

/* Mount a disk image (D64/D71/D81) into a virtual drive.
 *
 * drive_id - Virtual drive selector (UCI_DRIVE_8..UCI_DRIVE_11, or UCI_DRIVE_DEFAULT).
 * filename - Uppercase ASCII path to disk image.
 */
uci_err_t uci_dos_mount_disk(uci_drive_t drive_id, const char *filename);

/* Unmount the disk image from a virtual drive.
 *
 * drive_id - Virtual drive selector (UCI_DRIVE_8..UCI_DRIVE_11, or UCI_DRIVE_DEFAULT).
 */
uci_err_t uci_dos_unmount_disk(uci_drive_t drive_id);

/* Get size, date, and name of the currently open file.
 *
 * info - Receives file metadata.  info.filename is ASCII from firmware.
 */
uci_err_t uci_dos_file_info(uci_file_info_t *info);

/* Get size, date, and name of a file without opening it.
 *
 * filename - Uppercase ASCII.
 * info     - Receives file metadata.  info.filename is ASCII from firmware.
 */
uci_err_t uci_dos_file_stat(const char *filename, uci_file_info_t *info);

/* Flip to the other side of a dual-sided disk image (D71/D81).
 *
 * drive_id - Virtual drive selector (UCI_DRIVE_8..UCI_DRIVE_11, or UCI_DRIVE_DEFAULT).
 */
uci_err_t uci_dos_swap_disk(uci_drive_t drive_id);

/* Set the real-time clock.
 *
 * year  - Full 4-digit year, e.g. 2025.
 * month - 1-12.
 * day   - 1-31.
 * hour  - 0-23.
 * min   - 0-59.
 * sec   - 0-59.
 */
uci_err_t uci_dos_set_time(uint16_t year, uint8_t month, uint8_t day,
                           uint8_t hour, uint8_t min, uint8_t sec);

/* Switch to the home directory and return its path.
 *
 * buf     - Receives ASCII from firmware.
 * max_len - Size of buf.
 */
uci_err_t uci_dos_copy_home_path(char *buf, uint16_t max_len);

/* Network target functions */

/* Get network subsystem version string.
 *
 * buf     - Receives ASCII from firmware.
 * max_len - Size of buf.
 */
uci_err_t uci_net_identify(char *buf, uint16_t max_len);

/* How many network interfaces are available, and their names.
 *
 * count    - Receives the number of interfaces.
 * names    - Receives ASCII interface names from firmware.
 * max_len  - Size of names.
 */
uci_err_t uci_net_get_interface_count(uint8_t *count, char *names, uint16_t names_max);

/* Read the MAC address of a network interface.
 *
 * interface_id - Interface index (0-based).
 * mac          - Receives 6-byte MAC address.
 */
uci_err_t uci_net_get_mac(uint8_t interface_id, uint8_t mac[6]);

/* Read IP address, netmask, and gateway of a network interface.
 *
 * interface_id - Interface index (0-based).
 * ip           - Receives 4-byte IP address.
 * netmask      - Receives 4-byte subnet mask.
 * gateway      - Receives 4-byte gateway address.
 */
uci_err_t uci_net_get_ip(uint8_t interface_id, uint8_t ip[4], uint8_t netmask[4], uint8_t gateway[4]);

/* Open a TCP connection.
 *
 * hostname  - Hostname or IP string in ASCII encoding.
 * port      - TCP port number.
 * socket_id - Receives the socket handle.
 */
uci_err_t uci_net_open_tcp(const char *hostname, uint16_t port, uint8_t *socket_id);

/* Open a UDP socket.
 *
 * hostname  - Hostname or IP string in ASCII encoding.
 * port      - UDP port number.
 * socket_id - Receives the socket handle.
 */
uci_err_t uci_net_open_udp(const char *hostname, uint16_t port, uint8_t *socket_id);

/* Close a socket opened by uci_net_open_tcp or uci_net_open_udp.
 *
 * socket_id - Socket handle to close.
 */
uci_err_t uci_net_close_socket(uint8_t socket_id);

/* Read incoming data from a socket.
 *
 * socket_id  - Socket handle.
 * buf        - Receives raw data from the socket.  No encoding
 *              conversion is performed.  When exchanging text
 *              with a remote endpoint, be aware of the expected
 *              encoding.
 * max_len    - Size of buf.
 * bytes_read - Receives actual number of bytes read.
 */
uci_err_t uci_net_read_socket(uint8_t socket_id, uint8_t *buf, uint16_t max_len, uint16_t *bytes_read);

/* Send data to a socket.
 *
 * socket_id     - Socket handle.
 * buf           - Raw data to send.  No encoding conversion is
 *                 performed.  When exchanging text with a remote
 *                 endpoint, be aware of the expected encoding.
 * len           - Number of bytes to send.
 * bytes_written - Receives actual number of bytes sent.
 */
uci_err_t uci_net_write_socket(uint8_t socket_id, const uint8_t *buf, uint16_t len, uint16_t *bytes_written);

/* Control target functions */

/* Get control subsystem version string.
 *
 * buf     - Receives ASCII from firmware.
 * max_len - Size of buf.
 */
uci_err_t uci_ctrl_identify(char *buf, uint16_t max_len);

/* Get hardware info string (product name or raw SID response).
 *
 * device  - Device selector (UCI_HWINFO_PRODUCT or UCI_HWINFO_SID).
 * buf     - Receives string or binary response from firmware.
 * max_len - Size of buf.
 */
uci_err_t uci_ctrl_get_hwinfo(uci_hwinfo_dev_t device, char *buf, uint16_t max_len);

/* Get parsed SID hardware and emulated chip info.
 *
 * info - Receives structured SID count, base addresses, enabled status, and models.
 */
uci_err_t uci_ctrl_get_sid_info(uci_sid_info_t *info);

/* Get info about virtual disk drives.
 *
 * effective_id - Drive selector (UCI_DRIVE_8..UCI_DRIVE_11, or UCI_DRIVE_DEFAULT).
 * entries      - Receives parsed drive entries.
 * max_entries  - Maximum number of entries to store in entries array.
 * out_count    - Receives actual drive count available.
 */
uci_err_t uci_ctrl_get_drvinfo(uci_drive_t effective_id,
                             uci_drive_entry_t *entries, uint8_t max_entries, uint8_t *out_count);

/* Reboot the Ultimate device. */
uci_err_t uci_ctrl_reboot(void);

/* Turn on virtual drive A. */
uci_err_t uci_ctrl_enable_disk_a(void);

/* Turn off virtual drive A. */
uci_err_t uci_ctrl_disable_disk_a(void);

/* Turn on virtual drive B. */
uci_err_t uci_ctrl_enable_disk_b(void);

/* Turn off virtual drive B. */
uci_err_t uci_ctrl_disable_disk_b(void);

/* Check whether virtual drive A is powered on.
 *
 * powered_on - Receives true if drive A is on.
 */
uci_err_t uci_ctrl_disk_a_power(bool *powered_on);

/* Check whether virtual drive B is powered on.
 *
 * powered_on - Receives true if drive B is on.
 */
uci_err_t uci_ctrl_disk_b_power(bool *powered_on);

/* EasyFlash / RAMDisk */

/* Erase one sector of an EasyFlash cartridge (8 KB, filled with 0xFF).
 *
 * bank_info - Bits 3-5 pick the bank (0-7).
 * base_addr - Bit 5 set = high ROM ($A000), clear = low ROM ($8000).
 */
uci_err_t uci_ctrl_easyflash_erase_sector(uint8_t bank_info, uint8_t base_addr);

/* Read RAMDisk layout: 4 pairs of (type, size_mb).
 *
 * entries - Array of 4 RAMDisk entries to populate.
 */
uci_err_t uci_ctrl_get_ramdiskinfo(uci_ramdisk_entry_t entries[4]);

/* SoftIEC target functions */

/* Get SoftIEC subsystem version string.
 *
 * buf     - Receives ASCII from firmware.
 * max_len - Size of buf.
 */
uci_err_t uci_siec_identify(char *buf, uint16_t max_len);

/* High-level SoftIEC Directory Listing */

/* Open directory listing on a virtual drive (or default drive if UCI_DRIVE_DEFAULT). */
uci_err_t uci_siec_open_dir(uci_drive_t drive_id);

/* Read next directory entry.
 * Returns UCI_SUCCESS on entry read, or UCI_ERR_END_OF_LISTING when complete.
 * entry->filename receives PETSCII from device firmware.
 */
uci_err_t uci_siec_read_dir(uci_dir_entry_t *entry);

/* Close directory listing. */
uci_err_t uci_siec_close_dir(void);

/* Set up a file load: open the file and read the PRG header.
 *
 * Returns the start address (from the header, or load_addr if non-zero)
 * in *start_addr.  No data is transferred to C64 RAM yet.
 *
 * Call uci_siec_load_ex next to perform the DMA transfer.  The split
 * exists because the DMA halts the C64 CPU, so the caller needs the
 * target address before the transfer begins - to avoid overwriting its
 * own code, set up BASIC pointers, or chain multiple loads.
 *
 * sec_addr   - Secondary address (UCI_SEC_LOAD, etc.). 0 = use PRG header address
 *              unless load_addr overrides it.
 * verify     - If true, verify instead of load.
 * load_addr  - Override address for loading.  0 = use the 2-byte PRG
 *              header.  Non-zero = load at this address instead.
 * end_addr   - End address limit (not currently enforced by firmware;
 *              pass 0).
 * filename   - Uppercase ASCII path (e.g. "/TEMP/FILE.PRG").
 * start_addr - Receives the actual start address.
 */
uci_err_t uci_siec_load_su(uci_sec_addr_t sec_addr, bool verify, uint16_t load_addr,
                          uint16_t end_addr, const char *filename,
                          uint16_t *start_addr);

/* Execute the DMA transfer set up by uci_siec_load_su.
 *
 * Copies the file contents into C64 RAM at the address returned by
 * load_su.  The C64 CPU is halted for the duration of the transfer.
 *
 * sec_addr - Must match the sec_addr from the preceding load_su call.
 * verify   - If true, verify against memory instead of loading.
 * end_addr - Receives the address one past the last byte written,
 *            so the byte count is (end_addr - start_addr).
 */
uci_err_t uci_siec_load_ex(uci_sec_addr_t sec_addr, bool verify, uint16_t *end_addr);

/* Save a block of memory to a file.
 *
 * verify    - If true, verify after saving.
 * sec_addr  - Secondary address (UCI_SEC_SAVE, etc.).
 * start_addr - Start address of data to save.
 * end_addr   - End address of data to save.
 * filename   - Uppercase ASCII.  Prefix with '@' to overwrite.
 */
uci_err_t uci_siec_save(bool verify, uci_sec_addr_t sec_addr,
                       uint16_t start_addr, uint16_t end_addr,
                       const char *filename);

/* Open a file on the virtual drive.
 *
 * sec_addr - Secondary address (UCI_SEC_DIR for directory, etc.).
 * filename - Uppercase ASCII.
 */
uci_err_t uci_siec_open(uci_sec_addr_t sec_addr, const char *filename);

/* Close a file on the virtual drive.
 *
 * sec_addr - Secondary address from uci_siec_open.
 */
uci_err_t uci_siec_close(uci_sec_addr_t sec_addr);

/* Start reading from a file opened with sec_addr.  Also grabs the first bytes.
 *
 * sec_addr     - Secondary address from uci_siec_open.
 * prefetch     - Receives first bytes of file data.
 * prefetch_max - Size of prefetch buffer.
 * prefetch_len - Receives actual number of prefetch bytes.
 */
uci_err_t uci_siec_chkin(uci_sec_addr_t sec_addr, uint8_t *prefetch,
                        uint16_t prefetch_max, uint16_t *prefetch_len);

/* Start writing to a file opened with sec_addr.  Optionally send first bytes.
 *
 * sec_addr - Secondary address from uci_siec_open.
 * data     - Data to write (may be NULL if len is 0).
 * len      - Number of bytes to write.
 */
uci_err_t uci_siec_chkout(uci_sec_addr_t sec_addr, const uint8_t *data, uint16_t len);

/* Like uci_siec_load_su, but auto-mounts the disk image containing the file.
 *
 * sec_addr   - Secondary address (UCI_SEC_LOAD, etc.).
 * verify     - If true, verify instead of load.
 * load_addr  - Start address for loading.
 * end_addr   - End address for loading.
 * filename   - Uppercase ASCII.
 * start_addr - Receives the actual start address.
 */
uci_err_t uci_siec_load_mount_su(uci_sec_addr_t sec_addr, bool verify, uint16_t load_addr,
                                 uint16_t end_addr, const char *filename,
                                 uint16_t *start_addr);

/* Like uci_siec_load_ex, but for a load set up by load_mount_su.
 *
 * sec_addr - Secondary address from uci_siec_load_mount_su.
 * verify   - If true, verify instead of load.
 * end_addr - Receives the actual end address.
 */
uci_err_t uci_siec_load_mount_ex(uci_sec_addr_t sec_addr, bool verify, uint16_t *end_addr);

#endif /* UCI_H */
