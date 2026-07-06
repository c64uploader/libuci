/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * codec.c - Character encoding conversions for C64.
 *
 * Bridges the gap between the compiler's native encoding
 * (PETSCII under cc65, ASCII under oscar64) and the encoding
 * needed for network protocols, screen output, or firmware.
 *
 * See include/uci_codec.h for the public API.
 */

#include "uci_codec.h"

/* cc65 does not support __noinline; define it away. */
#if defined(__CC65__)
#define __noinline
#endif

/* Single-character conversions */

char uci_c_a(char c) {
    unsigned char uc = (unsigned char)c;
#if defined(__CC65__)
    /* cc65: native is PETSCII - reverse the inversion. */
    if (uc >= UCI_UC_MIN && uc <= UCI_UC_MAX) return (char)(uc - 0x80);
    if (uc >= UCI_LC_MIN && uc <= UCI_LC_MAX) return (char)(uc + 0x20);
#endif
    /* oscar64: native is ASCII - no conversion needed. */
    (void)uc;
    return c;
}

char uci_c_p(char c) {
    unsigned char uc = (unsigned char)c;
#if defined(__CC65__)
    /* cc65: strings are PETSCII, so ASCII must be explicitly marked.
     * Convert ASCII letters to PETSCII alt range (0xC1-0xDA) for
     * display via printf/BSOUT in the Lowercase/Uppercase charset. */
    if (uc >= 0x41 && uc <= 0x5A) return (char)(uc + 0x80);
    if (uc >= 0x61 && uc <= 0x7A) return (char)(uc - 0x20);
#endif
    /* oscar64: strings are ASCII, no conversion needed for network ASCII. */
    return c;
}

char uci_c_s(char c) {
    unsigned char uc = (unsigned char)c;
    /* Native -> screen code in the Lowercase/Uppercase character set. */
#if defined(__CC65__)
    if (uc >= 0xC1 && uc <= 0xDA) return (char)(uc - 0xA0);  /* PETSCII uppercase */
    if (uc >= 0x41 && uc <= 0x5A) return (char)(uc - 0x40);  /* PETSCII lowercase */
#else
    if (uc >= 0x41 && uc <= 0x5A) return (char)(uc - 0x20);  /* ASCII uppercase */
    if (uc >= 0x61 && uc <= 0x7A) return (char)(uc - 0x60);  /* ASCII lowercase */
#endif
    return c;
}

char uci_c_su(char c) {
    unsigned char uc = (unsigned char)c;
    /* Native -> screen code in the Uppercase/Graphics character set.
     * In this charset, only uppercase A-Z are available as letters
     * (screen codes 0x01-0x1A).  Both uppercase and lowercase source
     * letters map to uppercase display glyphs. */
#if defined(__CC65__)
    if (uc >= 0xC1 && uc <= 0xDA) return (char)(uc - 0xC0);  /* PETSCII uppercase */
    if (uc >= 0x41 && uc <= 0x5A) return (char)(uc - 0x40);  /* PETSCII lowercase -> uppercase screen */
#else
    if (uc >= 0x41 && uc <= 0x5A) return (char)(uc - 0x40);  /* ASCII uppercase */
    if (uc >= 0x61 && uc <= 0x7A) return (char)(uc - 0x60);  /* ASCII lowercase -> uppercase screen */
#endif
    return c;
}

char uci_c_au(char c) {
    unsigned char uc = (unsigned char)c;
#if defined(__CC65__)
    /* cc65: PETSCII uppercase (0xC1-0xDA) -> ASCII uppercase (0x41-0x5A).
     * PETSCII lowercase (0x41-0x5A) is already in ASCII uppercase range. */
    if (uc >= UCI_UC_MIN && uc <= UCI_UC_MAX) return (char)(uc - 0x80);
#else
    /* oscar64: ASCII lowercase (0x61-0x7A) -> ASCII uppercase (0x41-0x5A).
     * ASCII uppercase is already correct. */
    if (uc >= UCI_LC_MIN && uc <= UCI_LC_MAX) uc -= 0x20;
#endif
    return (char)uc;
}

char uci_c_d(char c) {
    unsigned char uc = (unsigned char)c;
#if defined(__CC65__)
    /* cc65: device data is standard PETSCII - convert to alt PETSCII
     * for printf display in the Lowercase/Uppercase charset. */
    if (uc >= 0x61 && uc <= 0x7A) return (char)(uc + 0x60);  /* standard upper -> alt upper */
    else if (uc >= 0x41 && uc <= 0x5A) return (char)(uc + 0x80);  /* standard lower -> alt upper */
    else if (uc >= 0xC1 && uc <= 0xDA) return (char)(uc);         /* alt upper - unchanged */
#else
    /* oscar64: device data is standard PETSCII - convert to ASCII,
     * which displays as readable letters via BSOUT in the
     * hardware-default Uppercase/Graphics charset. */
    if (uc >= 0x61 && uc <= 0x7A) return (char)(uc - 0x20);  /* standard upper -> ASCII upper */
    if (uc >= 0x41 && uc <= 0x5A) return (char)(uc);         /* standard lower -> ASCII upper (same byte!) */
    if (uc >= 0xC1 && uc <= 0xDA) return (char)(uc - 0x80);  /* alt upper -> ASCII upper */
#endif
    return c;
}

/* Buffer conversions: NUL-terminated src
 * Note: Uses index-based iteration (src[i]) instead of pointer increments (*dst++)
 * to work around an oscar64 -O3 optimization bug:
 * https://github.com/drmortalwombat/oscar64/issues/340
 */

__noinline char *uci_s_a(char *dst, const char *src) {
    char *out = dst;
#if defined(__CC65__)
    uint16_t i = 0;
    while (src[i]) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 0xC1 && c <= 0xDA)       c -= 0x80;
        else if (c >= 0x41 && c <= 0x5A)  c += 0x20;
        dst[i] = (char)c;
        i++;
    }
    dst[i] = '\0';
#else
    uint16_t i = 0;
    while (src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
#endif
    return out;
}

__noinline char *uci_s_au(char *dst, const char *src) {
    char *out = dst;
    uint16_t i = 0;
    while (src[i]) {
        unsigned char c = (unsigned char)src[i];
#if defined(__CC65__)
        /* cc65: PETSCII uppercase (0xC1-0xDA) -> ASCII uppercase (0x41-0x5A).
         * PETSCII lowercase (0x41-0x5A) is already in ASCII uppercase range. */
        if (c >= UCI_UC_MIN && c <= UCI_UC_MAX) c -= 0x80;
#else
        /* oscar64: ASCII lowercase (0x61-0x7A) -> ASCII uppercase (0x41-0x5A).
         * ASCII uppercase is already correct. */
        if (c >= UCI_LC_MIN && c <= UCI_LC_MAX) c -= 0x20;
#endif
        dst[i] = (char)c;
        i++;
    }
    dst[i] = '\0';
    return out;
}

__noinline char *uci_s_p(char *dst, const char *src) {
    char *out = dst;
    uint16_t i = 0;
    while (src[i]) {
        unsigned char c = (unsigned char)src[i];
#if defined(__CC65__)
        if (c >= 0x41 && c <= 0x5A)       c += 0x80;
        else if (c >= 0x61 && c <= 0x7A)  c -= 0x20;
#else
        /* oscar64: native is ASCII - straight copy */
#endif
        dst[i] = (char)c;
        i++;
    }
    dst[i] = '\0';
    return out;
}

__noinline char *uci_s_s(char *dst, const char *src) {
    char *out = dst;
    uint16_t i = 0;
    while (src[i]) {
        unsigned char c = (unsigned char)src[i];
#if defined(__CC65__)
        if (c >= 0xC1 && c <= 0xDA)       c -= 0xA0;
        else if (c >= 0x41 && c <= 0x5A)  c -= 0x40;
#else
        if (c >= 0x41 && c <= 0x5A)       c -= 0x20;
        else if (c >= 0x61 && c <= 0x7A)  c -= 0x60;
#endif
        dst[i] = (char)c;
        i++;
    }
    dst[i] = '\0';
    return out;
}

__noinline char *uci_s_su(char *dst, const char *src) {
    char *out = dst;
    uint16_t i = 0;
    while (src[i]) {
        unsigned char c = (unsigned char)src[i];
#if defined(__CC65__)
        if (c >= 0xC1 && c <= 0xDA)       c -= 0xC0;
        else if (c >= 0x41 && c <= 0x5A)  c -= 0x40;
#else
        if (c >= 0x41 && c <= 0x5A)       c -= 0x40;
        else if (c >= 0x61 && c <= 0x7A)  c -= 0x60;
#endif
        dst[i] = (char)c;
        i++;
    }
    dst[i] = '\0';
    return out;
}

__noinline char *uci_s_d(char *dst, const char *src) {
    char *out = dst;
    uint16_t i = 0;
    while (src[i]) {
        unsigned char c = (unsigned char)src[i];
#if defined(__CC65__)
        /* cc65: device data is standard PETSCII - convert to alt PETSCII
         * for printf display in the Lowercase/Uppercase charset. */
        if (c >= 0x61 && c <= 0x7A)       c += 0x60;  /* standard upper -> alt upper */
        else if (c >= 0x41 && c <= 0x5A)  c += 0x80;  /* standard lower -> alt upper */
        else if (c >= 0xC1 && c <= 0xDA)  ;           /* alt upper - unchanged */
#else
        /* oscar64: device data is standard PETSCII - convert to ASCII,
         * which displays as readable letters via BSOUT in the
         * hardware-default Uppercase/Graphics charset. */
        if (c >= 0x61 && c <= 0x7A)       c -= 0x20;  /* standard upper -> ASCII upper */
        else if (c >= 0xC1 && c <= 0xDA)  c -= 0x80;  /* alt upper -> ASCII upper */
        /* 0x41-0x5A: standard lower -> ASCII upper (same byte - unchanged) */
#endif
        dst[i] = (char)c;
        i++;
    }
    dst[i] = '\0';
    return out;
}

/* Length-bounded conversions (n bytes, no NUL) */

__noinline char *uci_m_a(char *dst, const char *src, uint16_t n) {
    char *out = dst;
    uint16_t i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)*src++;
#if defined(__CC65__)
        if (c >= 0xC1 && c <= 0xDA)       c -= 0x80;
        else if (c >= 0x41 && c <= 0x5A)  c += 0x20;
#endif
        *dst++ = (char)c;
    }
    return out;
}

__noinline char *uci_m_au(char *dst, const char *src, uint16_t n) {
    char *out = dst;
    uint16_t i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)*src++;
#if defined(__CC65__)
        if (c >= UCI_UC_MIN && c <= UCI_UC_MAX) c -= 0x80;
#else
        if (c >= UCI_LC_MIN && c <= UCI_LC_MAX) c -= 0x20;
#endif
        *dst++ = (char)c;
    }
    return out;
}

__noinline char *uci_m_p(char *dst, const char *src, uint16_t n) {
    char *out = dst;
    uint16_t i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)*src++;
#if defined(__CC65__)
        if (c >= 0x41 && c <= 0x5A)       c += 0x80;
        else if (c >= 0x61 && c <= 0x7A)  c -= 0x20;
#else
        if (c >= 0x61 && c <= 0x7A)       c -= 0x20;
#endif
        *dst++ = (char)c;
    }
    return out;
}

__noinline char *uci_m_s(char *dst, const char *src, uint16_t n) {
    char *out = dst;
    uint16_t i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)*src++;
#if defined(__CC65__)
        if (c >= 0xC1 && c <= 0xDA)       c -= 0xA0;
        else if (c >= 0x41 && c <= 0x5A)  c -= 0x40;
#else
        if (c >= 0x41 && c <= 0x5A)       c -= 0x20;
        else if (c >= 0x61 && c <= 0x7A)  c -= 0x60;
#endif
        *dst++ = (char)c;
    }
    return out;
}

__noinline char *uci_m_su(char *dst, const char *src, uint16_t n) {
    char *out = dst;
    uint16_t i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)*src++;
#if defined(__CC65__)
        if (c >= 0xC1 && c <= 0xDA)       c -= 0xC0;
        else if (c >= 0x41 && c <= 0x5A)  c -= 0x40;
#else
        if (c >= 0x41 && c <= 0x5A)       c -= 0x40;
        else if (c >= 0x61 && c <= 0x7A)  c -= 0x60;
#endif
        *dst++ = (char)c;
    }
    return out;
}

__noinline char *uci_m_d(char *dst, const char *src, uint16_t n) {
    char *out = dst;
    uint16_t i;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)*src++;
#if defined(__CC65__)
        if (c >= 0x61 && c <= 0x7A)       c += 0x60;
        else if (c >= 0x41 && c <= 0x5A)  c += 0x80;
        else if (c >= 0xC1 && c <= 0xDA)  ;
#else
        if (c >= 0x61 && c <= 0x7A)       c -= 0x20;
        else if (c >= 0xC1 && c <= 0xDA)  c -= 0x80;
#endif
        *dst++ = (char)c;
    }
    return out;
}
