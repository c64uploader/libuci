# C64 Character Handling for C Programmers

## ASCII vs PETSCII

C source code uses ASCII. The Commodore 64 uses PETSCII. These encodings differ:

| Source | ASCII | cc65 output | Screen Code |
|--------|-------|-------------|-------------|
| `'A'`  | 65    | 193 (0xC1)  | 33          |
| `'a'`  | 97    | 65 (0x41)   | 1           |
| `'G'`  | 71    | 199 (0xC7)  | 39          |
| `'g'`  | 103   | 71 (0x47)   | 7           |
| `'0'`  | 48    | 48 (0x30)   | 48          |

cc65 inverts case: uppercase source -> PETSCII 0xC1..0xDA range, lowercase source -> PETSCII 0x41..0x5A range. This inversion exists because BSOUT in the Lowercase/Uppercase charset maps PETSCII 0x41..0x5A to lowercase display and 0xC1..0xDA to uppercase display.

**Neither range matches network ASCII.** `'G'` becomes 0xC7, `'g'` becomes 0x47 - both wrong for HTTP where 'G' should be 0x47. Digits and punctuation are unchanged (`'0'` stays `0x30`).

If you write `"Hello"` in C, the bytes in memory depend on the compiler. This breaks portability, especially for **network protocol strings** (HTTP, etc.) that expect exact ASCII values.

## How Bytes Reach the Screen

When your C program calls `printf("Hello")`, the bytes travel through several steps:

1. **C source**: You type `"Hello"` in your source code (ASCII).
2. **Compiler**: The compiler converts ASCII to PETSCII and stores the bytes in the `.prg` file.
3. **Memory**: When the program runs, the PETSCII bytes are loaded into memory.
4. **printf**: The `printf` function sends the PETSCII bytes to the KERNAL routine `BSOUT` at `$FFD2`.
5. **BSOUT** converts PETSCII to screen codes and writes them to screen memory at `$0400`.
6. **Screen**: The VIC-II chip reads screen memory and displays the corresponding glyphs.

You never call BSOUT directly - `printf`, `putchar`, and `puts` all use it internally.

Screen memory at `$0400` holds **screen codes**, never PETSCII. If you write PETSCII bytes directly to `$0400`, the wrong glyphs appear.

## Three Encodings

| Encoding | Used For |
|----------|----------|
| **ASCII** | C source code (what you type), network protocols |
| **PETSCII** | String literals in memory after cc65 compilation. Also what you send to BSOUT / KERNAL I/O routines. |
| **Screen codes** | The byte values stored in screen memory at `$0400`. BSOUT converts PETSCII to screen codes. |

## How the Compilers Differ

**cc65** converts ASCII to PETSCII automatically for Commodore targets, and also switches the C64 to the Lowercase/Uppercase character set at startup. No extra code needed.

**oscar64** keeps strings as ASCII by default. Use `p"..."` or `P"..."` prefix for PETSCII strings, or `s"..."` prefix for screen codes. Unlike cc65, oscar64 does **not** switch character set mode.

## Quick Decision Matrix

| Task | cc65 | oscar64 |
|------|------|---------|
| printf of string literal | Default - just `printf("Hello")` | Use `printf(p"Hello")` |
| printf of received network data | `uci_s_p(buf, buf)` then `printf("%s", buf)` | `uci_s_p(buf, buf)` then `printf("%s", buf)` |
| Send string over network | `uci_s_a(buf, "GET...")` | `uci_s_a(buf, "GET...")` |
| Compare received byte against ASCII letter | `if (b == uci_c_a('G'))` | `if (b == uci_c_a('G'))` |
| Write to screen memory (cc65 default charset) | `uci_s_s(dst, src)` or `uci_m_s(dst, src, n)` | - |
| Write to screen memory (hardware default charset) | - | `uci_s_su(dst, src)` or `uci_m_su(dst, src, n)` |
| Display a device string (dir listing, path) | `uci_s_d(dst, src)` then `printf("%s", dst)` | `uci_s_d(dst, src)` then `printf("%s", dst)` |
| Filenames in libuci calls | Write as-is (lowercase works) | Write as-is (uppercase works) |

## Codec API

libuci provides functions to convert between compiler-native encoding and the encoding you need.

### Single-character functions

| Function | Converts | Use case |
|----------|----------|----------|
| `uci_c_a(c)` | compiler-native -> ASCII | Network protocol comparisons |
| `uci_c_p(c)` | ASCII -> PETSCII | `printf` display of a received byte |
| `uci_c_s(c)` | compiler-native -> screen code (Lowercase/Uppercase set) | Direct `$0400` writes under cc65 |
| `uci_c_su(c)` | compiler-native -> screen code (Uppercase/Graphics set) | Direct `$0400` writes in hardware-default charset |
| `uci_c_d(c)` | PETSCII (device) -> compiler-native | Display a byte from a firmware string |

`uci_c_a` reverses the cc65 PETSCII inversion, producing real ASCII bytes. Under oscar64 it's a no-op (strings stay ASCII).
`uci_c_p` converts ASCII bytes to PETSCII for `printf` display. Under oscar64 it's a no-op (ASCII works natively with BSOUT).
`uci_c_s` maps to screen codes in the Lowercase/Uppercase set (cc65's default charset).  See [Screen Code Helpers](#screen-code-helpers-and-character-sets) for the difference between `uci_c_s` and `uci_c_su`.
`uci_c_d` converts PETSCII bytes (from the Ultimate firmware) to whatever your compiler needs for display.

### Byte-value examples (cc65)

| Input | `uci_c_a` | `uci_c_p` | `uci_c_s` | `uci_c_su` | `uci_c_d` |
|-------|-----------|-----------|-----------|------------|----------|
| `'A'` (cc65: 0xC1) | 0x41 | - | 0x21 | 0x01 | - |
| `'a'` (cc65: 0x41) | 0x61 | - | 0x01 | 0x01 | - |
| 0x41 (ASCII 'A') | - | 0xC1 | - | - | - |
| 0x61 (ASCII 'a') | - | 0x41 | - | - | - |
| 0x41 (device lowercase) | - | - | - | - | 0xC1 |
| 0x61 (device uppercase) | - | - | - | - | 0xC1 |

`uci_c_a` and `uci_c_d` take compiler-native input. `uci_c_p` takes ASCII input. `uci_c_s`/`uci_c_su` take compiler-native input.
Under oscar64, all functions are no-ops or trivial mappings since native encoding is ASCII.

### Buffer functions (NUL-terminated src)

All copy from `src` to `dst`, converting each character. `dst` and `src` may point to the same buffer for in-place conversion. Returns `dst` (like `strcpy`).

| Function | Converts | Use case |
|----------|----------|----------|
| `uci_s_a(dst, src)` | compiler-native -> ASCII | Prepare a network request |
| `uci_s_p(dst, src)` | ASCII -> PETSCII | Prepare received network data for `printf` |
| `uci_s_s(dst, src)` | compiler-native -> screen code (Lowercase/Uppercase) | Write to `$0400` under cc65 |
| `uci_s_su(dst, src)` | compiler-native -> screen code (Uppercase/Graphics) | Write to `$0400` in hardware-default charset |
| `uci_s_d(dst, src)` | PETSCII (device) -> compiler-native | Display a firmware string (dir listing, path) |

### Length-bounded buffer functions (n bytes, no NUL)

These copy exactly `n` bytes from `src` to `dst`, converting each character. They **do not** NUL-terminate `dst`. Use them when the source data isn't NUL-terminated (e.g. buffer from a network read). Returns `dst`.

| Function | Converts | Use case |
|----------|----------|----------|
| `uci_m_a(dst, src, n)` | compiler-native -> ASCII | Network send of non-NUL-terminated data |
| `uci_m_p(dst, src, n)` | ASCII -> PETSCII | Display received data that isn't NUL-terminated |
| `uci_m_s(dst, src, n)` | compiler-native -> screen code (Lowercase/Uppercase) | Direct screen write with length |
| `uci_m_su(dst, src, n)` | compiler-native -> screen code (Uppercase/Graphics) | Direct screen write with length |
| `uci_m_d(dst, src, n)` | PETSCII (device) -> compiler-native | Display firmware data with length |

## Network Protocol Strings

With the codec API, you write readable strings and convert to ASCII at runtime:

```c
char http_get[256];
uci_s_a(http_get, "GET / HTTP/1.0\r\n"
                "Host: example.com\r\n"
                "Connection: close\r\n\r\n");
uci_net_write_socket(sock, (uint8_t *)http_get,
                     strlen(http_get), &written);
```

For single characters in protocol parsing:

```c
if (buf[i] == '<' && buf[i+1] == uci_c_a('i')) { ... }
```

### Rules for network strings

1. Pass readable strings with letter literals; call `uci_s_a()` before sending.
2. Use `uci_c_a()` for single-character comparisons.
3. Punctuation and digits (`'/'`, `'.'`, `' '`, `'1'`, `'0'`) are the same in both ASCII and PETSCII - they work without conversion.
4. For non-NUL-terminated data (e.g. partial network reads), use `uci_m_a()`.

## Network to Screen - Displaying Received Data

When you receive ASCII data from a network socket and want to print it via `printf`, use `uci_c_p()` or `uci_s_p()` to convert to PETSCII:

```c
/* Single character */
printf("%c", uci_c_p(buf[i]));

/* Whole buffer (in-place, NUL-terminated) */
uci_s_p((char *)recv_buf, (char *)recv_buf);
printf("%s", (char *)recv_buf);

/* Partial buffer (not yet NUL-terminated) */
uci_m_p((char *)recv_buf, (char *)recv_buf, bytes_received);
recv_buf[bytes_received] = '\0';
printf("%s", (char *)recv_buf);
```

The mapping:

| Received byte | `uci_c_p` output | Screen display |
|---|---|---|
| `0x41..0x5A` (ASCII A-Z) | `0xC1..0xDA` (cc65) / unchanged (oscar64) | Uppercase |
| `0x61..0x7A` (ASCII a-z) | `0x41..0x5A` (cc65) / unchanged (oscar64) | Lowercase |
| other | unchanged | as-is |

### The full network pattern

```c
/* 1. Build request from readable string */
char buf[256];
uci_s_a(buf, "GET / HTTP/1.0\r\n");          /* native -> ASCII */
uci_net_write_socket(sock, (uint8_t *)buf, strlen(buf), &written);

/* 2. Read response */
uint16_t len;
uci_net_read_socket(sock, (uint8_t *)buf, sizeof(buf) - 1, &len);
buf[len] = '\0';

/* 3. Display */
uci_s_p(buf, buf);                             /* ASCII -> PETSCII */
printf("Response: %s\n", buf);
```

If you want to avoid the intermediate NUL-termination, use `uci_m_p`:

```c
uint16_t len;
uci_net_read_socket(sock, (uint8_t *)buf, sizeof(buf), &len);
uci_m_p(buf, buf, len);
buf[len] = '\0';  /* now NUL-terminate for printf */
printf("Response: %s\n", buf);
```

### Hostname character handling

`uci_net_open_tcp()` and `uci_net_open_udp()` automatically convert the input hostname string using `uci_s_a()` internally. You can pass compiler-native mixed-case string literals directly under both cc65 and oscar64:

```c
/* Automatic ASCII conversion is performed by uci_net_open_tcp / uci_net_open_udp */
uci_net_open_tcp("ip-api.com", 80, &socket_id);
```

## Device Data to Native - Displaying Firmware Strings

The Ultimate firmware returns directory listings, path names, and status strings in **standard PETSCII** (0x41..0x5A = lowercase, 0x61..0x7A = uppercase). To display these correctly via `printf`, use `uci_s_d()` or `uci_c_d()`:

```c
/* Whole string (NUL-terminated) */
char path[256];
uci_dos_copy_home_path(path, sizeof(path));
char display[256];
uci_s_d(display, path);
printf("HOME: %s\n", display);

/* Single character */
printf("%c", uci_c_d(firmware_byte));
```

The mapping depends on your compiler:

| Received byte (standard PETSCII) | Meaning | `uci_c_d` under cc65 | `uci_c_d` under oscar64 |
|----------------------------------|---------|---------------------|------------------------|
| `0x61..0x7A` | Uppercase letters A-Z | `0xC1..0xDA` (alt PETSCII uppercase) | `0x41..0x5A` (ASCII uppercase) |
| `0x41..0x5A` | Lowercase letters a-z | `0xC1..0xDA` (alt PETSCII uppercase) | `0x41..0x5A` (ASCII uppercase) |
| `0xC1..0xDA` | Alt PETSCII uppercase | unchanged (alt PETSCII is already native) | `0x41..0x5A` (ASCII uppercase) |

Under both compilers, the result displays as uppercase letters through `printf`/BSOUT.

## Screen Code Helpers and Character Sets

The Commodore 64's Character ROM contains two character sets. The screen code for a given letter depends on which set is active.

### Which charset is active?

| Compiler | Active charset at startup |
|----------|--------------------------|
| **cc65** | Lowercase/Uppercase Set (switched via `crt0.s`) |
| **oscar64** | Uppercase/Graphics Set (hardware default) |

**Note:** cc65's `crt0.s` switches to the Lowercase/Uppercase set at startup (via `lda #14 / jsr BSOUT`). Under cc65, `uci_c_s()` is the correct function for direct screen writes. Under oscar64, use `uci_c_su()`. Both produce screen codes 0x01-0x1A for letters, but they render differently:

- Lowercase/Uppercase (cc65): displays as lowercase
- Uppercase/Graphics (oscar64): displays as uppercase

### Which function to use

| Charset | Single char | Buffer (NUL-terminated) | Buffer (length-bounded) |
|---------|-------------|------------------------|------------------------|
| Lowercase/Uppercase (cc65) | `uci_c_s(c)` | `uci_s_s(dst, src)` | `uci_m_s(dst, src, n)` |
| Uppercase/Graphics (oscar64, hardware default) | `uci_c_su(c)` | `uci_s_su(dst, src)` | `uci_m_su(dst, src, n)` |

### Lowercase/Uppercase set (`uci_c_s` / `uci_s_s`)

- `'A'` -> screen 0x21 (uppercase A on screen)
- `'a'` -> screen 0x01 (lowercase a on screen)

### Uppercase/Graphics set (`uci_c_su` / `uci_s_su`)

- `'A'` -> screen 0x01 (uppercase A on screen)
- `'a'` -> screen 0x01 (uppercase A on screen - **lowercase not available in this charset**)

Both functions are safe to use in either compiler. Use the one that matches your active charset.

## Direct Screen Memory

Screen RAM ($0400) uses **screen codes**, not PETSCII. Choose your charset:

```c
unsigned char *screen = (unsigned char *)0x0400;
```

**Under cc65** (Lowercase/Uppercase charset):

```c
screen[0] = uci_c_s('A');  // -> 33 (uppercase A)
screen[1] = uci_c_s('h');  // -> 8  (lowercase h)
uci_s_s((char *)screen, "Hello");
```

**Under oscar64** (hardware-default Uppercase/Graphics charset):

```c
screen[0] = uci_c_su('A');  // -> 1 (uppercase A)
screen[1] = uci_c_su('h');  // -> 8 (uppercase A - lowercase not available)
uci_s_su((char *)screen, "Hello");
```

**Note:** Under cc65, `crt0.s` switches to Lowercase/Uppercase at startup, so `uci_c_s()` is correct. Under oscar64, use `uci_c_su()` for the default Uppercase/Graphics charset. Both functions produce the same screen codes (0x01-0x1A for letters) - only the display case differs.

In oscar64, you can also use the `s"..."` prefix for screen codes in either charset:

```c
memcpy(screen, s"HELLO", 5);
```

## Converting Strings (The Right Way)

### cc65

```c
// printf - just works, conversion is automatic
printf("Hello\n");

// Network - convert to ASCII before sending
uci_s_a(buf, "GET / HTTP/1.0\r\n");

// Screen memory - convert to screen codes
uci_s_s((char *)0x0400, "Hello");
```

### oscar64

```c
// printf - use p"..." prefix for PETSCII strings
printf(p"Hello\n");

// Network - convert to ASCII before sending (same as cc65)
uci_s_a(buf, "GET / HTTP/1.0\r\n");

// Screen memory - use s"..." prefix or uci_s_su
memcpy((void*)0x0400, s"HELLO", 5);
uci_s_su((char *)0x0400, "Hello");
```

### What not to do

Avoid switching the hardware character set mode with `putchar(14)` or `putchar(142)`. This is a **global side effect** that changes how BSOUT maps PETSCII to screen codes for the entire program:

```c
// Discouraged: global state change, fragile, unnecessary
putchar(14);                // switches VIC-II to Lowercase/Uppercase set
printf(p"Hello\n");
```

The codec API and string prefixes handle encoding correctly without touching hardware state. They compose safely with other code.

## Character Literals

Character literals follow the same rules as strings:

```c
char c = 'a';  // ASCII 97 in source
               // cc65: becomes PETSCII 193
               // oscar64: stays 97 (unless -psci)
```

For portable comparisons, use the single-character codec functions:

```c
/* Compare a received ASCII byte against an expected letter */
if (buf[i] == uci_c_a('G')) { ... }

/* Compare a PETSCII byte (from keyboard, file, etc.) against a letter */
if (key == uci_c_p('G')) { ... }
```

Or use numeric constants for hardcoded values:

```c
#define ASCII_G 0x47
if (buf[i] == ASCII_G) { ... }
```

## `uci_c_p` / `uci_s_p` Under oscar64

Under **cc65**, `uci_c_p` converts ASCII bytes to the alt PETSCII range (0xC1-0xDA for uppercase) for display via `printf`/BSOUT in the Lowercase/Uppercase charset.

Under **oscar64**, `uci_c_p` is a **no-op** (returns the byte unchanged). ASCII bytes work natively with BSOUT in the hardware-default Uppercase/Graphics Set, because:

- ASCII uppercase (0x41-0x5A) occupies the same byte range as **PETSCII lowercase**
- BSOUT maps PETSCII lowercase to screen codes 1–26
- In the Uppercase/Graphics Set, screen codes 1–26 display as **uppercase letters**

### Critical: ASCII lowercase displays as graphics

In the hardware-default **Uppercase/Graphics Set**, ASCII **lowercase** letters (0x61-0x7A) map through BSOUT to screen codes 0x41-0x5A (64–95), which are **graphics symbols**, not readable letters:

| Format string byte | BSOUT screen code | Uppercase/Graphics Set display |
|--------------------|-------------------|-------------------------------|
| ASCII `'T'` (0x54) | 0x14 (20) | Uppercase `'T'` ✓ |
| ASCII `'t'` (0x74) | 0x54 (84) | Graphics symbol ✗ |

**Rule for oscar64 printf format strings & dynamic data:**
- **Format strings:** Use UPPERCASE letters. Mixed-case string literals like `"CHDIR /temp…"` display `"/temp"` as graphics. Write `"CHDIR /TEMP…"` instead.
- **Dynamic received ASCII data:** When printing received mixed-case ASCII strings (such as JSON payload values like `"Finland"`), pass the extracted value through `uci_s_au(value, value)` before calling `printf()`. This converts lowercase ASCII to uppercase ASCII (`"FINLAND"`), ensuring BSOUT renders readable letters (screen codes 1–26) instead of graphics symbols.

## Filename & Path Character Encoding

The Ultimate firmware accepts path and filename strings in either alt PETSCII (0x41..0x5A for letters, as produced by cc65 lowercase source) or plain ASCII (as produced by oscar64). Both work:

```c
/* Under cc65: 'M' -> 0xCD (PETSCII), 'y' -> 0x59 (PETSCII)
 * Under oscar64: 'M' -> 0x4D (ASCII),    'y' -> 0x79 (ASCII)
 * Both work - firmware converts to FAT format automatically. */
uci_dos_mount_disk(8, "/temp/MyFile.d64");
```

No conversion needed. The library sends bytes as-is.

### Firmware preserves byte values

The Ultimate firmware stores and returns filenames using the **exact byte values** it receives. No normalization happens. This means `strcmp(filename, expected)` comparisons work when both sides use the same encoding.

## Common Pitfalls

**1. Lowercase displays as graphics (oscar64 only)**
```c
// Wrong: no string prefix - ASCII bytes go to BSOUT which interprets
// them as PETSCII, producing wrong screen codes
printf("hello\n");

// Right: use p"" prefix for PETSCII strings with oscar64
printf(p"hello\n");
```

cc65 programs start in the Lowercase/Uppercase set automatically and do not need `p""` prefixes.

**2. Using putchar(14) as a workaround (discouraged)**
```c
// Discouraged: switching the hardware character set globally
// is unnecessary when the codec API handles encoding correctly.
// This also breaks on cc65 (see below).
putchar(14);
printf(p"hello\n");

// Instead: just use p"" and let BSOUT do its job
printf(p"hello\n");
```

**3. Switching to Uppercase/Graphics set breaks cc65 output**
```c
// Wrong: cc65 strings are PETSCII expecting the Lowercase/Uppercase set.
// Switching to Uppercase/Graphics set changes how BSOUT maps PETSCII->screen codes.
putchar(142);  // switch to Uppercase/Graphics set
printf("PASS\n");  // displays as garbage
```

**4. PETSCII in screen memory**
```c
// Wrong: PETSCII 193 displays as ♠
screen[0] = 193;

// Right: use uci_c_s or uci_c_su depending on your active charset
screen[0] = uci_c_s('A');   // cc65: screen code 33 (uppercase A)
screen[0] = uci_c_su('A');  // oscar64: screen code 1 (uppercase A)
```

**5. Using the wrong screen-code helper**
```c
// uci_c_s assumes Lowercase/Uppercase set. On oscar64 (hardware default
// Uppercase/Graphics set), screen code 33 is NOT the letter 'A'.
screen[0] = uci_c_s('A');  // wrong under oscar64

// Use uci_c_su for the hardware-default charset:
screen[0] = uci_c_su('A');  // correct under oscar64
```

**6. Underscore displays as <- (left arrow)**

ASCII underscore `_` is byte `0x5F`. In PETSCII, `0x5F` is the left arrow symbol (<-), and the C64 Character ROM has no underscore glyph in either character set. Any underscore in a string literal passed to `printf` or BSOUT will display as `<-`.

```c
// Wrong: displays as "SEC<-ADDR" on C64
printf("  SIEC OPEN SEC_ADDR...\n");

// Right: use dashes instead
printf("  SIEC OPEN SEC-ADDR...\n");
```

This is not a codec issue - the codec correctly preserves byte `0x5F`. The C64 simply cannot display an underscore in any character mode.

**7. Character comparisons differ between compilers**
```c
// Breaks when porting
if (key == 'a') { ... }

// Better: use the codec functions
if (key == uci_c_a('a')) { ... }      // compare against ASCII
if (key == uci_c_p('a')) { ... }      // compare against PETSCII
```

## Quick Reference

### String encoding

| Task | cc65 | oscar64 |
|------|------|---------|
| PETSCII strings | Default | `p"..."` prefix or `-psci` |
| Screen codes | `uci_s_s(dst, src)` | `s"..."` prefix or `uci_s_su(dst, src)` |
| Lowercase mode | Automatic (crt0.s) | Not needed (`p"..."` + BSOUT) |

### Codec functions

| Task | Code | Memory |
|------|------|--------|
| Send "GET" over network | `uci_s_a(dst, "GET...")` | dst buffer |
| Send raw data (not NUL-terminated) | `uci_m_a(dst, src, n)` | dst buffer |
| Display received network data | `uci_s_p(buf, buf)` + `printf` | in-place, 0 extra |
| Display single network byte | `printf("%c", uci_c_p(c))` | 0 RAM/ROM |
| Display device string (path, dir) | `uci_s_d(dst, src)` + `printf` | dst buffer |
| Write to screen (cc65) | `uci_s_s(dst, src)` or `uci_m_s()` | 0 extra RAM |
| Write to screen (oscar64) | `uci_s_su(dst, src)` or `uci_m_su()` | 0 extra RAM |
| Compare against ASCII | `if (c == uci_c_a('X'))` | 0 RAM/ROM |
| Filename in libuci calls | Write as-is | - |

## Background: cc65's Automatic Charset Switch

Every cc65 CBM target (C64, C128, Plus/4, PET, VIC-20) switches to the Lowercase/Uppercase character set at startup. This is hardcoded in each target's `crt0.s`:

```asm
; From cc65 libsrc/c64/crt0.s
        lda     #14          ; PETSCII $0E = switch to lowercase/uppercase charset
        jsr     BSOUT        ; emit via KERNAL CHROUT ($FFD2)
```

This is unconditional and happens for every C program.

### Why?

C source code is mixed-case (`printf("Hello World")`). The C64's hardware-default charset (Uppercase/Graphics Set) has no lowercase glyphs - there is no way to display both `'H'` and `'e'` as letters. The only charset with both uppercase and lowercase is the Lowercase/Uppercase Set.

### Practical implications

- **You never need `putchar(14)` in cc65 programs.** The charset is already switched.
- **Custom crt0:** If you replace `crt0.s` (e.g. via `-C c64-asm.cfg`), the charset switch is your responsibility.
- **Screen decoding:** Screen codes 1–26 are lowercase letters in the Lowercase/Uppercase Set, but uppercase letters in the Uppercase/Graphics Set. The Go test harness correctly maps both ranges to uppercase ASCII for reliable `PASS`/`FAIL` detection regardless of character set.

## Background: oscar64 `-psci` Flag and Direct Screen Writes

When compiling with `oscar64`:

- Passing `-psci` converts all string literals to PETSCII at compile time. This is safe if your program only uses `printf`/`putchar` (which go through BSOUT). BSOUT converts PETSCII to screen codes, producing correct output in the hardware-default charset.

- **If you write directly to screen memory** (`$0400`), PETSCII bytes are wrong - they land as arbitrary screen codes. Without `-psci`, ASCII bytes are also wrong: ASCII uppercase (65–90) written to `$0400` become screen codes 65–90, which are graphics symbols in the Uppercase/Graphics Set.

**Solution**: use screen-code helpers or the `s"..."` prefix:

```c
// Direct screen write - use the right tool for your charset:
uci_s_su((char *)0x0400, "HELLO");        // Uppercase/Graphics set (oscar64 default)
memcpy((void *)0x0400, s"HELLO", 5);    // oscar64 s"" prefix
```
