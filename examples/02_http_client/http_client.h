/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/02_http_client/http_client.h - Tiny HTTP GET client
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdint.h>
#include "uci.h"

/* Perform an HTTP GET request and retrieve the response.
 *
 * host        - Target hostname or IP string (e.g. "ip-api.com").
 * port        - Target TCP port (e.g. 80).
 * path        - Resource path (e.g. "/json").
 * buf         - Caller-allocated memory buffer to hold request & response.
 * max_len     - Total size of buf (e.g. 512).
 * body        - Optional: receives pointer to HTTP body start inside buf.
 * status_code - Optional: receives HTTP status code (e.g. 200, 404).
 *
 * Returns UCI_SUCCESS on network success, or error code on failure.
 * Memory efficient: zero dynamic allocation; uses caller-supplied buf for all IO.
 */
uci_err_t http_get(const char *host, uint16_t port, const char *path,
                 char *buf, uint16_t max_len, char **body, uint16_t *status_code);

#endif /* HTTP_CLIENT_H */
