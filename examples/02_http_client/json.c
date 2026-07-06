/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * examples/02_http_client/json.c - Tiny JSON string value extractor
 */

#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "json.h"

const char *json_get(const char *json, const char *key, char *dst, uint8_t max_len) {
    const char *p = json;
    uint8_t klen;
    uint8_t i;

    if (!json || !key || !dst || max_len == 0) {
        return NULL;
    }

    klen = (uint8_t)strlen(key);
    while ((p = strstr(p, key)) != NULL) {
        /* Verify key is bounded by unescaped quotes: "key" */
        if (p > json && p[-1] == '"' && p[klen] == '"' && (p <= json + 1 || p[-2] != '\\')) {
            p = strchr(p + klen, ':');
            if (!p) break;
            p++;
            /* Skip whitespace and optional opening quote */
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '"') {
                p++;
            }
            i = 0;
            /* Copy value, unescaping simple backslash escape sequences */
            while (*p && *p != '"' && *p != ',' && *p != '}' && *p != ']' && i < max_len - 1) {
                if (*p == '\\' && p[1]) {
                    p++; /* Skip escape backslash */
                }
                dst[i++] = *p++;
            }
            /* Trim trailing whitespace for primitive non-quoted values */
            while (i > 0 && (dst[i - 1] == ' ' || dst[i - 1] == '\t' || dst[i - 1] == '\r' || dst[i - 1] == '\n')) {
                i--;
            }
            dst[i] = '\0';
            return dst;
        }
        p += klen;
    }

    return NULL;
}
