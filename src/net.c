/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * net.c - Network access through the Ultimate device.
 *
 * Functions for opening TCP and UDP connections to remote hosts,
 * reading and writing socket data, and querying the device's
 * own IP address, netmask, gateway, and MAC address.
 */

#include "uci.h"
#include "internal.h"

static unsigned int local_strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (unsigned int)(p - s);
}

/* Network command IDs */
#define NET_CMD_IDENTIFY            0x01
#define NET_CMD_GET_INTERFACE_COUNT 0x02
#define NET_CMD_GET_MAC             0x04
#define NET_CMD_GET_IP     0x05
#define NET_CMD_OPEN_TCP   0x07
#define NET_CMD_OPEN_UDP   0x08
#define NET_CMD_CLOSE      0x09
#define NET_CMD_READ       0x10
#define NET_CMD_WRITE      0x11

uci_err_t uci_net_identify(char *buf, uint16_t max_len) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_IDENTIFY);
    if (res != UCI_SUCCESS) return res;
    res = uci_execute_cmd((uint8_t *)buf, max_len - 1, &reply_len);
    if (res == UCI_SUCCESS) {
        buf[reply_len] = '\0';
    }
    return res;
}

uci_err_t uci_net_get_interface_count(uint8_t *count, char *names, uint16_t names_max) {
    uint16_t total_read;
    uci_err_t res;
    uci_iov_t iovs[2];
    res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_GET_INTERFACE_COUNT);
    if (res != UCI_SUCCESS) return res;
    iovs[0].buf = count;  iovs[0].cap = 1;
    iovs[1].buf = (uint8_t *)names;  iovs[1].cap = names_max ? names_max - 1 : 0;
    res = uci_execute_cmd_v(iovs, 2, &total_read);
    if (res != UCI_SUCCESS) return res;
    if (total_read < 1) return UCI_ERR_PARAM;
    if (names && names_max > 0) {
        uint16_t name_len = (total_read > 1) ? total_read - 1 : 0;
        if (name_len >= names_max) name_len = names_max - 1;
        names[name_len] = '\0';
    }
    return UCI_SUCCESS;
}

uci_err_t uci_net_get_mac(uint8_t interface_id, uint8_t mac[6]) {
    uint16_t reply_len;
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_GET_MAC);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(interface_id);
    return uci_execute_cmd(mac, 6, &reply_len);
}

uci_err_t uci_net_get_ip(uint8_t interface_id, uint8_t ip[4], uint8_t netmask[4], uint8_t gateway[4]) {
    uint16_t total_read;
    uci_err_t res;
    uci_iov_t iovs[3];
    res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_GET_IP);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(interface_id);
    iovs[0].buf = ip;       iovs[0].cap = 4;
    iovs[1].buf = netmask;  iovs[1].cap = 4;
    iovs[2].buf = gateway;  iovs[2].cap = 4;
    res = uci_execute_cmd_v(iovs, 3, &total_read);
    if (res == UCI_SUCCESS && total_read < 12) return UCI_ERR_PARAM;
    return res;
}

uci_err_t uci_net_open_tcp(const char *hostname, uint16_t port, uint8_t *socket_id) {
    uint16_t reply_len;
    uci_err_t res;
    unsigned int len;

    if (!hostname || !socket_id) return UCI_ERR_PARAM;
    len = local_strlen(hostname);
    if (len > UCI_HOSTNAME_MAX) return UCI_ERR_PARAM;

    res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_OPEN_TCP);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(port & 0xFF);
    uci_write_cmd_byte((port >> 8) & 0xFF);
    uci_write_cmd_bytes((const uint8_t *)hostname, len + 1);

    return uci_execute_cmd(socket_id, 1, &reply_len);
}

uci_err_t uci_net_open_udp(const char *hostname, uint16_t port, uint8_t *socket_id) {
    uint16_t reply_len;
    uci_err_t res;
    unsigned int len;

    if (!hostname || !socket_id) return UCI_ERR_PARAM;
    len = local_strlen(hostname);
    if (len > UCI_HOSTNAME_MAX) return UCI_ERR_PARAM;

    res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_OPEN_UDP);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(port & 0xFF);
    uci_write_cmd_byte((port >> 8) & 0xFF);
    uci_write_cmd_bytes((const uint8_t *)hostname, len + 1);

    return uci_execute_cmd(socket_id, 1, &reply_len);
}

uci_err_t uci_net_close_socket(uint8_t socket_id) {
    uci_err_t res;
    res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_CLOSE);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(socket_id);
    return uci_execute_cmd(NULL, 0, NULL);
}

uci_err_t uci_net_read_socket(uint8_t socket_id, uint8_t *buf, uint16_t max_len, uint16_t *bytes_read) {
    uci_err_t res;
    uint16_t total_read;
    uint16_t actual_len;
    uint8_t retries;
    uint8_t hdr[2];
    uci_iov_t iovs[2];

    if (max_len == 0) return UCI_ERR_PARAM;
    retries = 0;

    iovs[0].buf = hdr;  iovs[0].cap = 2;
    iovs[1].buf = buf;  iovs[1].cap = max_len;

    while (1) {
        res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_READ);
        if (res != UCI_SUCCESS) { if (bytes_read) *bytes_read = 0; return res; }
        uci_write_cmd_byte(socket_id);
        uci_write_cmd_byte(max_len & 0xFF);
        uci_write_cmd_byte((max_len >> 8) & 0xFF);

        /* The firmware prepends a 2-byte length prefix to the reply.
         * Route those 2 bytes into hdr[] and the payload directly into
         * the caller's buffer, so the caller gets exactly max_len bytes. */
        res = uci_execute_cmd_v(iovs, 2, &total_read);

        if (res == UCI_ERR_STATUS) {
            uint8_t code = uci_last_status_code();
            if (code == 2 && retries++ < 200) continue;
            /*
             * Status 1 = "CONNECTION CLOSED BY HOST". The firmware may
             * still include a final data payload in this same reply, so
             * fall through to parse the length prefix instead of
             * returning 0.
             */
            if (code != 1) { if (bytes_read) *bytes_read = 0; return res; }
        } else if (res != UCI_SUCCESS) {
            if (bytes_read) *bytes_read = 0;
            return res;
        }

        actual_len = 0;
        if (total_read >= 2) {
            actual_len = hdr[0] | (hdr[1] << 8);
            if (actual_len > total_read - 2) actual_len = total_read - 2;
        }
        if (bytes_read) *bytes_read = actual_len;
        return UCI_SUCCESS;
    }
}

uci_err_t uci_net_write_socket(uint8_t socket_id, const uint8_t *buf, uint16_t len, uint16_t *bytes_written) {
    uint8_t temp[2];
    uint16_t reply_len;
    uci_err_t res;

    res = uci_start_cmd(UCI_TARGET_NETWORK, NET_CMD_WRITE);
    if (res != UCI_SUCCESS) return res;
    uci_write_cmd_byte(socket_id);
    uci_write_cmd_bytes(buf, len);

    res = uci_execute_cmd(temp, 2, &reply_len);
    if (res == UCI_SUCCESS && reply_len >= 2) {
        if (bytes_written) {
            *bytes_written = temp[0] | (temp[1] << 8);
        }
    } else {
        if (bytes_written) *bytes_written = 0;
    }
    return res;
}