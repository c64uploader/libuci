/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/02_http_client/json.h - Tiny JSON string value extractor
 */

#ifndef JSON_H
#define JSON_H

#include <stddef.h>
#include <stdint.h>

/* Find key in JSON string and extract value into dst.
 *
 * json    - NUL-terminated JSON payload.
 * key     - Key name to search for (e.g. "city").
 * dst     - Caller-allocated buffer to receive extracted value string.
 * max_len - Size of dst buffer.
 *
 * Returns dst on success, or NULL if key is not found.
 * Memory efficient: zero dynamic allocations and uses caller-provided memory.
 */
const char *json_get(const char *json, const char *key, char *dst, uint8_t max_len);

#endif /* JSON_H */
