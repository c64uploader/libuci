/*
 * Copyright 2026 The libuci Authors
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/c64uploader/libuci
 *
 * uci_codec.h - Convert strings between ASCII, PETSCII, and screen codes.
 *
 * The firmware uses ASCII.  String literals in your source code compile
 * to different encodings depending on the compiler:
 *   cc65:    PETSCII  (e.g. "A" = $C1)
 *   oscar64: ASCII    (e.g. "A" = $41)
 *
 * These functions convert between the three encodings:
 *   ASCII      - Firmware filenames, paths, network data
 *   PETSCII    - C64 KERNAL display (printf, BSOUT)
 *   Screen code - Direct screen memory ($0400+)
 *
 * Naming convention:
 *   uci_c_{suffix}   - single character (1 byte)
 *   uci_s_{suffix}   - NUL-terminated string (like strcpy)
 *   uci_m_{suffix}   - memory block with length (like memcpy)
 *
 * Suffixes:
 *   a   - to ASCII (mixed case)
 *   au  - to uppercase ASCII (for filenames/paths)
 *   p   - to PETSCII
 *   s   - to screen code (Lowercase/Uppercase charset)
 *   su  - to screen code (Uppercase/Graphics charset, uppercase-only)
 *   d   - firmware reply to displayable string (for printf)
 *
 * All buffer functions copy from src to dst, converting every byte.
 * dst and src may overlap for in-place conversion.  Returns dst.
 */

#ifndef UCI_CODEC_H
#define UCI_CODEC_H

#include <stddef.h>
#include <stdint.h>

/* Byte ranges the compiler produces for letter literals.
 *   cc65:    uppercase 0xC1-0xDA,  lowercase 0x41-0x5A
 *   oscar64: uppercase 0x41-0x5A,  lowercase 0x61-0x7A
 */
#if defined(__CC65__)
  #define UCI_UC_MIN  0xC1
  #define UCI_UC_MAX  0xDA
  #define UCI_LC_MIN  0x41
  #define UCI_LC_MAX  0x5A
#else
  #define UCI_UC_MIN  0x41
  #define UCI_UC_MAX  0x5A
  #define UCI_LC_MIN  0x61
  #define UCI_LC_MAX  0x7A
#endif

/* Single-character conversions */

/* String literal -> ASCII (mixed case). */
char uci_c_a(char c);

/* ASCII -> PETSCII (for printf/BSOUT display). */
char uci_c_p(char c);

/* String literal -> uppercase ASCII (for filenames/paths). */
char uci_c_au(char c);

/* String literal -> screen code (Lowercase/Uppercase charset).
 * CAUTION: only correct in the Lowercase/Uppercase charset (cc65).
 * For the Uppercase/Graphics charset (hardware default), use uci_c_su(). */
char uci_c_s(char c);

/* String literal -> screen code (Uppercase/Graphics charset).
 * Letters -> screen 0x01-0x1A (uppercase only; lowercase not available). */
char uci_c_su(char c);

/* Firmware reply -> displayable string (for printf). */
char uci_c_d(char c);

/* Buffer functions: NUL-terminated src */

/* String literal -> ASCII (mixed case).  For network hostnames. */
char *uci_s_a(char *dst, const char *src);

/* ASCII -> PETSCII.  For displaying received ASCII data via printf. */
char *uci_s_p(char *dst, const char *src);

/* String literal -> uppercase ASCII.  For firmware filenames/paths. */
char *uci_s_au(char *dst, const char *src);

/* String literal -> screen code (Lowercase/Uppercase charset). */
char *uci_s_s(char *dst, const char *src);

/* String literal -> screen code (Uppercase/Graphics charset). */
char *uci_s_su(char *dst, const char *src);

/* Firmware reply -> displayable string (for printf). */
char *uci_s_d(char *dst, const char *src);

/* Buffer functions: length-bounded (n bytes, no NUL) */

/* Like uci_s_a but processes exactly n bytes, no NUL terminator. */
char *uci_m_a(char *dst, const char *src, uint16_t n);

/* Like uci_s_au but processes exactly n bytes, no NUL terminator. */
char *uci_m_au(char *dst, const char *src, uint16_t n);

/* Like uci_s_p but processes exactly n bytes, no NUL terminator. */
char *uci_m_p(char *dst, const char *src, uint16_t n);

/* Like uci_s_s but processes exactly n bytes, no NUL terminator. */
char *uci_m_s(char *dst, const char *src, uint16_t n);

/* Like uci_s_su but processes exactly n bytes, no NUL terminator. */
char *uci_m_su(char *dst, const char *src, uint16_t n);

/* Like uci_s_d but processes exactly n bytes, no NUL terminator. */
char *uci_m_d(char *dst, const char *src, uint16_t n);

#endif /* UCI_CODEC_H */
