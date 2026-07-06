# UCI (Ultimate Command Interface) - C Programming Reference

> [!NOTE]
> Official Command Interface documentation can be found at
> * https://1541u-documentation.readthedocs.io/en/latest/command%20interface.html
> * https://github.com/GideonZ/1541ultimate/tree/master/doc


## Prerequisites

The UCI registers are mapped into the C64's I/O space. This mapping is **optional** and must be enabled in the Ultimate settings:

1. Enter the Ultimate menu
2. Go to **Memory & ROMs**
3. Set **Command Interface** to **Enabled**


## Hardware Register Reference

### Base Address Relocability

The UCI base address is not fixed. The system automatically relocates it based on the active cartridge to avoid I/O conflicts. Your code must handle these common addresses:

| Mode | Base Address | Notes |
| :--- | :--- | :--- |
| **Standard** | `$DF1C` | Default for most scenarios and standard cartridges. |
| **EasyFlash** | `$DE1C` | Used when EasyFlash is active. |
| **High** | `$DFFC` | Used for SID player and special modes. |

To detect which address window is active at runtime, see [Dynamic Register Window Probing](#1-dynamic-register-window-probing-df1c-vs-de1c) in the C Client implementation guidelines below.


### Register Addresses

The UCI hardware registers are memory-mapped relative to the active base address (`Base`):

| Offset | Address (Standard) | Name | Direction | Description |
| :--- | :--- | :--- | :--- | :--- |
| `Base - 1` | `$DF1B` | `OUR_DEVICE` | Read/Write | Device ID register. Identifies target device. |
| `Base + 0` | `$DF1C` | `CMD_IF_CONTROL` / `CMD_IF_STATUS` | Write / Read | Control register (write) and Status register (read). |
| `Base + 1` | `$DF1D` | `CMD_IF_COMMAND` | Write / Read | Command data output register (write), or signature identification register (read). |
| `Base + 2` | `$DF1E` | `CMD_IF_RESULT` | Read | Response data buffer input register. Read payload bytes here. |
| `Base + 3` | `$DF1F` | `CMD_IF_STATUS_MSG` | Read | Status message buffer input register. Read status string bytes here. |


### Control & Status Register Bit Definitions

Writing to or reading from `Base + 0` (`$DF1C` by default) accesses the Control and Status registers:

#### Control Register (Write to `Base + 0`)

| Bit | Hex Value | Name | Description |
| :--- | :--- | :--- | :--- |
| Bit 0 | `0x01` | `CTL_PUSH_CMD` | Push/execute command. Triggers command processing. |
| Bit 1 | `0x02` | `CTL_DATA_ACC` | Acknowledge data chunk read. Requests next block from firmware. |
| Bit 2 | `0x04` | `CTL_ABORT` | Abort current operation and request state reset. |
| Bit 3 | `0x08` | `CTL_CLR_ERR` | Clear error flag. |

#### Status Register (Read from `Base + 0`)

| Bit(s) | Hex Mask / Value | Name | Description |
| :--- | :--- | :--- | :--- |
| Bit 0 | `0x01` | `CMD_BUSY` | Command pending in hardware queue. |
| Bit 1 | `0x02` | `DATA_ACC` | Data acknowledge status (reflects control write). |
| Bit 2 | `0x04` | `ABORT_P` | Abort operation pending. |
| Bit 3 | `0x08` | `ERROR` | Error occurred during command execution. |
| Bits 4–5 | `0x30` | `STAT_STATE_BITS` | Protocol state mask (see state values below). |
| | `0x00` | `STAT_STATE_IDLE` | State 00: IDLE - Ready for a new command. |
| | `0x10` | `STAT_STATE_BUSY` | State 01: BUSY - Firmware is processing command. |
| | `0x20` | `STAT_STATE_LAST` | State 10: DATA_LAST - Final (or only) data block available. |
| | `0x30` | `STAT_STATE_MORE` | State 11: DATA_MORE - Data block available, more blocks follow. |
| Bit 6 | `0x40` | `STAT_STAT_AV` | Status text data available to read at `Base + 3` (`$DF1F`). |
| Bit 7 | `0x80` | `STAT_DATA_AV` | Response data available to read at `Base + 2` (`$DF1E`). |

#### Buffer Size Limits

| Buffer | Max Bytes | Description |
| :--- | :--- | :--- |
| Command Buffer | 896 bytes | Maximum size of a command payload sent to hardware. |
| Reply Buffer | 896 bytes | Maximum size of a single reply data payload read from hardware. |
| Status Buffer | 256 bytes | Maximum size of a text status string read from hardware. |

## Communication Protocol

### Target Subsystems

Commands are dispatched to different internal firmware targets using the Target ID byte (Byte 0):

| Target ID | Name | Description |
| :--- | :--- | :--- |
| `0x01` / `0x02` | DOS Target | File operations, directory browsing, disk mounting, RTC. |
| `0x03` | Network Target | Network interface configuration, TCP/UDP sockets. |
| `0x04` | Control Target | System actions (reboot, freeze, REU control, hardware info). |
| `0x05` | Kernal / SoftIEC Target | KERNAL-level intercepts and SoftIEC channel operations. |

### General Message Structure

All command messages sent to the Command Register (`Base + 1`) follow this layout:

- **Byte 0**: Target ID (subsystem selector)
- **Byte 1**: Command ID (operation code within target)
- **Bytes 2+**: Command parameters / payload (if required by command)

**Example Command Packet:**
To query the IP address on interface 0 via the Network Target:
- Byte 0: `0x03` (Network Target)
- Byte 1: `0x05` (`NET_CMD_GET_IPADDR`)
- Byte 2: `0x00` (Interface 0)
- Full byte sequence written to `$DF1D`: `[0x03, 0x05, 0x00]`

You can find the message structure routing implementation in [software/io/command_interface/command_intf.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/io/command_interface/command_intf.cc) and [software/io/command_interface/command_intf.h](https://github.com/GideonZ/1541ultimate/blob/master/software/io/command_interface/command_intf.h).


### State Machine States

The Command Interface uses a 2-bit state field in the Status Register (bits 4–5) to indicate progress during command execution:
* **`IDLE` (`0x00`)**: Ready to receive a new command.
* **`BUSY` (`0x10`)**: Command accepted; firmware is processing the request.
* **`DATA_LAST` (`0x20`)**: Data response is available; this is the final (or only) block.
* **`DATA_MORE` (`0x30`)**: Data response is available; additional blocks will follow.

Once a command transitions into the `BUSY` state, data transfer proceeds using one of two protocols described below (Multi-Chunk Pull or Single-Transaction Push).


### Multi-Chunk Pull Protocol (Flow-Controlled)

Used when reading large data that exceeds a single 512-byte response chunk.
* **Where it is used**:
  * **DOS Target (`0x01`/`0x02`)**: `DOS_CMD_READ_DATA` (`0x04`) and `DOS_CMD_READ_DIR` (`0x14`).
  * **Kernal Target (`0x05`)**: Reading emulated SoftIEC channel data.
* **Where it is NOT used**: All other targets/commands (including all writes and all Network reads).

**Steps**:
1. **Send Command**: Write the command to `$DF1D` (`CMD_IF_COMMAND`) and write `CTL_PUSH_CMD` (`0x01`) to `$DF1C` (`CMD_IF_CONTROL`). State becomes `BUSY`.
2. **Wait**: Poll `$DF1C` until the state becomes `DATA_MORE` (`0x30`) or `DATA_LAST` (`0x20`).
3. **Read Chunk**: Read data bytes from `$DF1E` (`CMD_IF_RESULT`) while bit 7 (`DATA_AV`) of the status register is set.
4. **Acknowledge**: Write `CTL_DATA_ACC` (`0x02`) to `$DF1C` to tell the interface you have read the chunk.
   * If state was `DATA_MORE`, the state returns to `BUSY` while the firmware prepares the next chunk. Go back to step 2.
   * If state was `DATA_LAST`, the state returns to `IDLE`. The transfer is complete.


### Single-Transaction Push Protocol

Used for commands that send data (writes), simple query commands (identify, status, etc.), and all Network Target (`0x03`) operations.
* **Writes**: No pull flow control is needed because the firmware can process writes faster than the C64 can send them. Send your data payload embedded directly inside the command packet (up to 892 bytes of data payload per command).
* **Network Reads**: `NET_CMD_READ_SOCKET` (`0x10`) returns only a single, self-contained response chunk (up to 894 bytes) immediately flagged as `DATA_LAST`.

**Steps**:
1. **Send Command & Data**: Write the command (and any data payload) to `$DF1D`.
2. **Trigger**: Write `CTL_PUSH_CMD` (`0x01`) to `$DF1C`. The state becomes `BUSY`, then transitions directly to `DATA_LAST`.
3. **Read Reply (if any)**: Read reply bytes from `$DF1E` while bit 7 (`DATA_AV`) is set.
4. **Acknowledge**: Write `CTL_DATA_ACC` (`0x02`) to `$DF1C` to return the state directly to `IDLE`.
5. **Repeat**: Repeat steps 1–4 for subsequent blocks.


## Robust Client Implementation Guidelines

Implementing a client for the Ultimate Command Interface requires careful handling of C64 cartridge memory mapping, compiler optimizations, and hardware states.

### 1. Dynamic Register Window Probing ($DF1C vs $DE1C)
To allow a single client binary to work across all cartridge configurations without recompilation, your code must dynamically probe for the active register window at boot time.

You can detect the correct base address by probing the **Command Register** (`Base + 1`). Reading this register returns the signature value **`0xC9`** or **`0x49`** when the UCI interface is present and active at that address.

**Detection Logic:**
1. Read address `$DF1D` (Standard + 1). If `0xC9` or `0x49`, Base is `$DF1C`.
2. Read address `$DE1D` (EasyFlash + 1). If `0xC9` or `0x49`, Base is `$DE1C`.
3. Read address `$DFFD` (High + 1). If `0xC9` or `0x49`, Base is `$DFFC`.
4. If neither value is found at any of these locations, the UCI is **disabled** or not mapped.

**Example C Code Snippet:**

```c
#include <stdint.h>
#include <stdbool.h>

#define UCI_BASE_STANDARD   0xDF1C
#define UCI_BASE_EASYFLASH  0xDE1C
#define UCI_BASE_HIGH       0xDFFC

// Check if a base address contains the UCI identification signature
static bool probe_uci_base(uint16_t base) {
    // Read the Command Register (Base + 1)
    volatile uint8_t *cmd_reg = (volatile uint8_t *)(base + 1);
    uint8_t val = *cmd_reg;
    return (val == 0xC9 || val == 0x49);
}

// Automatically detect the active UCI base address.
// Returns the base address, or 0 if the interface is disabled or not mapped.
uint16_t detect_uci_base(void) {
    if (probe_uci_base(UCI_BASE_STANDARD))  return UCI_BASE_STANDARD;
    if (probe_uci_base(UCI_BASE_EASYFLASH)) return UCI_BASE_EASYFLASH;
    if (probe_uci_base(UCI_BASE_HIGH))      return UCI_BASE_HIGH;
    return 0; // UCI disabled/not present
}
```

### 2. Volatile Registers are Critical (C Language)
In C, all hardware registers must be declared with the `volatile` qualifier (e.g. `volatile uint8_t *`) to prevent the compiler optimization pass from caching register reads in CPU registers or removing status-polling loops.

### 3. Open-Bus `$FF` Latch Protection (CRITICAL)
If the Command Interface is disabled in the Ultimate settings, or if runtime cartridge bank switching moves or hides the I/O window mid-execution, reading unmapped I/O addresses returns **`$FF`** (`255`) due to C64 bus floating (open-bus behavior).
* **The Hazard**: The data-available flag (`STAT_DATA_AV`) is bit 7 (`0x80`). If an unmapped register returns `$FF`, `0xFF & 0x80` evaluates to non-zero (true). An unbounded polling loop like `while (is_data_available())` will lock up in an infinite loop, filling RAM with `$FF` until the C64 crashes.
* **The Mitigation**: Always explicitly check for `$FF` open-bus returns in your status checks:
    ```c
    uint8_t s = *UCI_STATUS_REG;
    if (s == 0xFF) return false; // Open-bus safety check: hardware absent or unmapped
    return (s & UCI_STAT_DATA_AV) != 0;
    ```

### 4. Bounded Wait Loops (No Infinite Tight Loops)
Never use unbounded `while` loops (e.g. `while(status & mask);`) to wait for status transitions. If the hardware registers enter an unresponsive state or become unmapped, the client will lock up infinitely.
* Implement bounded loop counters (`UCI_POLL_LIMIT = 60000U` or `65535U`) to allow timeout escapes.
* **Compiler Optimization Impact**: Stack-heavy compilers (like `cl65`) execute polling loops ~10x slower than register-optimized compilers (like `oscar64 -O3`). A small poll limit (e.g. `2000U`) that works under `cl65` (~15–20ms) will expire in ~2ms under `oscar64`, causing slow FAT filesystem operations (such as `rename` or `open`) to hit `UCI_ERR_TIMEOUT` prematurely. Always size `UCI_POLL_LIMIT` to ensure a timeout duration of at least 0.5–1.0s under maximum compiler optimization.
* If a poll limit is hit or if `$FF` is read consecutively for `1024` cycles, flag the interface as failed and short-circuit further accesses.

### 5. Recovery from Stale Command State
If a previous application run crashed or left unread data in the Ultimate's internal buffers, the command pipeline remains locked. Pushing new commands will fail or deadlock.
* At client startup, perform a **drain sequence**: write `CTL_ABORT` (`0x04`) to the control register, pulse `CTL_CLR_ERR` (`0x08`) if an error bit is set, and poll the status register until state returns to `IDLE` (`0x00`).

### 6. ROM-Safe State Variables (BSS Mapping)
When targeting cartridge formats (such as EasyFlash or CRT images), some C compilers place initialized global and static variables directly into read-only ROM space. Attempting to write to them at runtime to track state or offsets will silently fail.
* **The Solution**: Declare state-tracking variables **uninitialized** so they are placed in RAM (`.bss` segment), and initialize them dynamically at runtime in your startup code.

### 7. Cartridge Mapping Requirements (EasyFlash Subtype 1)
When compiling code to run as an EasyFlash cartridge, you must configure the build for **Subtype 1 (REU-aware EasyFlash)** (e.g., passing `-csub=1` in Oscar64).
* **Reason**: Standard EasyFlash (Subtype 0) claims all `$DF00–$DFFF` addresses for cartridge RAM, hiding the default UCI window at `$DF1C`. Subtype 1 directs the Ultimate firmware to relocate the UCI interface to `$DE1C–$DE1F`.

### 8. System Timing & Source Code References
* **Execution Timing**: Command execution times vary from microseconds (for quick status queries) to milliseconds (for SD card file I/O or network requests).
* **Abort Handshake Delay**: Writing `CTL_ABORT` (`0x04`) requests an abort, but the hardware state does not transition back to `IDLE` (`0x00`) instantaneously. The ARM coprocessor must process the abort request in software. After sending an abort command, clients must poll the status register until the state returns to `IDLE` (`0x00`) before issuing new commands.

**Firmware Source References:**
For developers interested in the underlying hardware and firmware implementation:
* Hardware state machine: [command_protocol.vhd](https://github.com/GideonZ/1541ultimate/blob/master/fpga/io/command_interface/vhdl_source/command_protocol.vhd)
* Register mapping & base selection: [c64.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/io/c64/c64.cc) and [c64_crt.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/io/c64/c64_crt.cc)
* Command processing & routing: [command_intf.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/io/command_interface/command_intf.cc) and [command_intf.h](https://github.com/GideonZ/1541ultimate/blob/master/software/io/command_interface/command_intf.h)



## UCI DOS Command Message Structures

All DOS commands target DOS Target `0x01` (or `0x02` alias).

### File Operation Commands:

#### **0x01 - DOS_CMD_IDENTIFY**
```
Command:   [0x01, 0x01]
Length:   2 bytes
Reply:    "ULTIMATE-II DOS V1.2" (20 bytes)
Status:   "00,OK"
```

#### **0x02 - DOS_CMD_OPEN_FILE**
```
Command:  [0x01, 0x02, mode, filename...]
Length:   3+ bytes
          - mode: File open mode (see File Open Mode Flags table below)
          - filename: Null-terminated filename string
Reply:    Empty
Status:   "00,OK" on success, or raw FatFS error string on failure (e.g. "FILE DOESN'T EXIST" - lacks the "NN," prefix).
Notes:    Opens a file in the current path. Only one file can be open at a time.
```

File Open Mode Flags:

| Hex Value | Mode | Description |
| :--- | :--- | :--- |
| `0x01` | `FA_READ` | Read access |
| `0x00` | `FA_OPEN_EXISTING` | Open existing file (default) |
| `0x02` | `FA_WRITE` | Write access (file should exist if no create flags set) |
| `0x04` | `FA_CREATE_NEW` | Create new file (file should not exist yet) |
| `0x08` | `FA_CREATE_ALWAYS` | Create or overwrite file (clears contents) |
| `0x0C` | `FA_CREATE_ANY` | Create new or create always (`FA_CREATE_NEW \| FA_CREATE_ALWAYS`) |
| `0x0E` | `FA_ANY_WRITE_FLAG` | Any write flag (`FA_CREATE_NEW \| FA_CREATE_ALWAYS \| FA_WRITE`) |
| `0x10` | `FA_OPEN_ALWAYS` | Open existing or create new file |
| `0x80` | `FA_OPEN_FROM_CBM` | Special flag for CBM filesystem access |

Flags can be combined with bitwise OR, e.g.:
  - Read-only:       `FA_READ` (`0x01`)
  - Write new file:  `FA_WRITE | FA_CREATE_ALWAYS` (`0x0A`)
  - Read-write:      `FA_READ | FA_WRITE | FA_OPEN_EXISTING` (`0x03`)
  - Write/create:    `FA_WRITE | FA_OPEN_ALWAYS` (`0x12`)

#### **0x03 - DOS_CMD_CLOSE_FILE**
```
Command:  [0x01, 0x03]
Length:   2 bytes
Reply:    Empty
Status:   "00,OK" or "84,NO FILE TO CLOSE"
```

#### **0x04 - DOS_CMD_READ_DATA**
```
Command:  [0x01, 0x04, length_low, length_high]
Length:   4 bytes
          - length:  Number of bytes to read (little-endian)
Reply:    [file_data... ] (variable length, may span multiple messages)
Status:   Empty (length 0) on success, or raw FatFS error string on failure.
Notes:    Uses the Multi-Chunk Pull Protocol (see top of document). The maximum chunk size returned is 512 bytes, but the initial request length can be larger.
```

#### **0x05 - DOS_CMD_WRITE_DATA**
```
Command:  [0x01, 0x05, unused, unused, data...]
Length:   4+ bytes
          - data: Bytes to write (length of data is implicit: command message length - 4)
Reply:    Empty
Status:   "00,OK" or "85,NO FILE OPEN" or file system error
Notes:    Uses the Single-Transaction Push Protocol (see top of document). The length fields at bytes 2-3 are ignored/unused by the firmware. Up to 892 bytes of data payload can be written per command. To write large files, send multiple commands sequentially.
```

#### **0x06 - DOS_CMD_FILE_SEEK**
```
Command:  [0x01, 0x06, pos0, pos1, pos2, pos3]
Length:   6 bytes
          - pos:  32-bit file position (little-endian)
Reply:    Empty
Status:   "00,OK" or "85,NO FILE OPEN" or error
```

#### **0x07 - DOS_CMD_FILE_INFO**
```
Command:  [0x01, 0x07]
Length:   2 bytes
Reply:    [size(4), date(2), time(2), ext(3), attrib(1), filename...]
          - size: 32-bit file size (little-endian)
          - date: 16-bit DOS date (little-endian)
          - time: 16-bit DOS time (little-endian)
          - ext: 3-byte extension
          - attrib: File attributes
          - filename: Filename (not null-terminated in reply; length is determined from reply message length)
Status:   "00,OK" or "85,NO FILE OPEN"
Notes:    Returns info about currently open file.
```

#### **0x08 - DOS_CMD_FILE_STAT**
```
Command:  [0x01, 0x08, filename...]
Length:   3+ bytes
          - filename: Null-terminated filename string
Reply:    [size(4), date(2), time(2), ext(3), attrib(1), filename...]
          - filename: Filename (not null-terminated in reply; length is determined from reply message length)
Status:   "00,OK" or "82,FILE NOT FOUND"
Notes:    Returns file info without opening the file.
```

#### **0x09 - DOS_CMD_DELETE_FILE**
```
Command:  [0x01, 0x09, filename...]
Length:   3+ bytes
          - filename: Null-terminated filename string
Reply:    Empty
Status:   "00,OK" or file system error
```

#### **0x0A - DOS_CMD_RENAME_FILE**
```
Command:  [0x01, 0x0A, oldname\0, newname\0]
Length:    4+ bytes
          - oldname:  Null-terminated old filename
          - newname: Null-terminated new filename
Reply:    Empty
Status:   "00,OK" or file system error
```

#### **0x0B - DOS_CMD_COPY_FILE**
```
Command:  [0x01, 0x0B, filename\0, destination\0]
Length:   4+ bytes
          - filename:  Null-terminated source filename (in current directory)
          - destination: Null-terminated destination directory path (MUST be absolute path, e.g., "/usb0/copydir").
Reply:    Empty
Status:    "00,OK" or file system error
Notes:     Copies the file into the destination directory under its original filename (does not support copying to a new filename). Because the firmware resolves the destination path relative to the virtual root partition `/`, a relative path will fail with "path doesn't exist".
```

### Directory Operation Commands:

#### **0x11 - DOS_CMD_CHANGE_DIR**
```
Command:  [0x01, 0x11, path...]
Length:   3+ bytes
          - path: Null-terminated directory path (can be relative or absolute)
Reply:    Empty
Status:   "00,OK" or "83,NO SUCH DIRECTORY"
```

#### **0x12 - DOS_CMD_GET_PATH**
```
Command:  [0x01, 0x12]
Length:   2 bytes
Reply:    [current_path... ] (not null-terminated string; length is determined from reply message length)
Status:   "00,OK"
```

#### **0x13 - DOS_CMD_OPEN_DIR**
```
Command:  [0x01, 0x13]
Length:   2 bytes
Reply:    Empty
Status:   "00,OK" or "86,CAN'T READ DIRECTORY" or "01,DIRECTORY EMPTY"
Notes:    Prepares directory listing for reading with READ_DIR
```

#### **0x14 - DOS_CMD_READ_DIR**
```
Command:  [0x01, 0x14]
Length:   2 bytes
Reply:    [attrib(1), filename...]
          - attrib: File attributes
          - filename: Filename (not null-terminated; length is reply message length - 1)
Status:   "00,OK" (last entry only) or empty (intermediate entries)
Notes:    Uses the Multi-Chunk Pull Protocol (see top of document) to stream directory entries. Note that the entry structure contains only the attribute byte and the filename, NOT the full stats structure.
```

#### **0x15 - DOS_CMD_COPY_UI_PATH**
```
Command:  [0x01, 0x15]
Length:   2 bytes
Reply:    Empty
Status:   "99,FUNCTION NOT IMPLEMENTED"
```

#### **0x16 - DOS_CMD_CREATE_DIR**
```
Command:  [0x01, 0x16, dirname...]
Length:   3+ bytes
          - dirname: Null-terminated directory name
Reply:    Empty
Status:   "00,OK" or file system error
```

#### **0x17 - DOS_CMD_COPY_HOME_PATH**
```
Command:  [0x01, 0x17]
Length:   2 bytes
Reply:    [home_path...] (not null-terminated string; length is determined from reply message length)
Status:   "00,OK" or error
Notes:    Changes to home directory and falls through to GET_PATH to return the path.
```

### REU/Memory Commands:

#### **0x21 - DOS_CMD_LOAD_REU**
```
Command:  [0x01, 0x21, addr0, addr1, addr2, addr3, length0, length1, length2, length3]
Length:   10 bytes
          - addr:  32-bit REU memory address (little-endian)
          - length: 32-bit number of bytes to load (little-endian)
Reply:    "$%6x BYTES LOADED TO REU $%6x" (35-byte text string)
Status:   "00,OK" or "85,NO FILE OPEN" or error
Notes:    Loads file data directly into REU memory
```

#### **0x22 - DOS_CMD_SAVE_REU**
```
Command:  [0x01, 0x22, addr0, addr1, addr2, addr3, length0, length1, length2, length3]
Length:   10 bytes
          - addr: 32-bit REU memory address (little-endian)
          - length: 32-bit number of bytes to save (little-endian)
Reply:    "$%6x BYTES SAVED FROM REU $%6x" (36-byte text string)
Status:   "00,OK" or "85,NO FILE OPEN" or error
Notes:    Saves REU memory directly to file
```

### Disk Image Commands:

#### **0x23 - DOS_CMD_MOUNT_DISK**
```
Command:  [0x01, 0x23, drive_id, filename...]
Length:   4+ bytes
          - drive_id: IEC device address of the drive (typically 8–11), or 0 for the default/last active drive
          - filename: Null-terminated disk image filename
Reply:    Empty
Status:   "00,OK" or error codes:
          - "82,FILE NOT FOUND"
          - "89,NOT A DISK IMAGE"
          - "90,DRIVE NOT PRESENT"
          - "91,INCOMPATIBLE IMAGE"
```

#### **0x24 - DOS_CMD_UMOUNT_DISK**
```
Command:  [0x01, 0x24, drive_id]
Length:   3 bytes
          - drive_id: Drive number to unmount
Reply:    Empty
Status:   "00,OK" or "90,DRIVE NOT PRESENT"
```

#### **0x25 - DOS_CMD_SWAP_DISK**
```
Command:  [0x01, 0x25, drive_id]
Length:   3 bytes
          - drive_id: Drive number to swap disk sides
Reply:    Empty
Status:    "00,OK" or "90,DRIVE NOT PRESENT"
Notes:    Swaps between sides of dual-sided disk images (D71, D81)
```

### Time Commands:

#### **0x26 - DOS_CMD_GET_TIME**
```
Command:  [0x01, 0x26, format]
Length:   2 or 3 bytes
          - format: Optional format selector (0 = date/time string, 1 = date/time with weekday prefix string)
Reply:    Format 0: "YYYY/MM/DD HH:MM:SS" (19-byte text string)
          Format 1: "WD YYYY/MM/DD HH:MM:SS" (23-byte text string, e.g., "SUN 2026/06/20 09:22:05")
Status:   "00,OK"
```

#### **0x27 - DOS_CMD_SET_TIME**
```
Command:  [0x01, 0x27, year, month, day, hour, min, sec]
Length:   8 bytes
          - year: Years since 1900 (e.g., 124 for year 2024)
          - month: 1-12
          - day: 1-31
          - hour:  0-23
          - min: 0-59
          - sec: 0-59
Reply:    [formatted_time... ] (23-byte text string, same as GET_TIME format 1)
Status:   "00,OK" or "21,UNKNOWN COMMAND" (returned on incorrect message length)
```

### RAMDisk Commands:

#### **0x41 - CTRL_CMD_LOAD_INTO_RAMDISK**
```
Command:  [0x01, 0x41, drive_id, filename...]
Length:   4+ bytes
          - drive_id: RAMDisk drive number (bit 7 set = "what-if" mode)
          - filename: Null-terminated disk image filename
Reply:    Empty
Status:   "00,OK" or error
Notes:    Loads disk image into RAM disk
```

#### **0x42 - CTRL_CMD_SAVE_RAMDISK**
```
Command:  [0x01, 0x42, drive_id, filename...]
Length:   4+ bytes
          - drive_id: RAMDisk drive number
          - filename: Null-terminated destination filename
Reply:    Empty
Status:   "00,OK" or error
Notes:     Saves RAM disk contents to disk image file
```

### Utility Commands:

#### **0xF0 - DOS_CMD_ECHO**
```
Command:  [0x01, 0xF0, data...]
Length:   2+ bytes
          - data: Any data to echo back
Reply:    Verbatim copy of the input command message (including target and command bytes)
Status:   "00,OK"
Notes:    Test command that echoes back the input.
```

### Common DOS Error Status Messages:
- `"00,OK"` - Success
- `"01,DIRECTORY EMPTY"` - No entries in directory
- `"02,REQUEST TRUNCATED"` - Request was truncated
- `"21,UNKNOWN COMMAND"` - Unrecognized command or incorrect message length
- `"81,NOT IN DATA MODE"` - Command requires data mode first
- `"82,FILE NOT FOUND"` - File does not exist
- `"83,NO SUCH DIRECTORY"` - Directory does not exist
- `"84,NO FILE TO CLOSE"` - No file is currently open
- `"85,NO FILE OPEN"` - Operation requires an open file
- `"86,CAN'T READ DIRECTORY"` - Directory read failed
- `"87,INTERNAL ERROR"` - Internal error occurred
- `"88,NO INFORMATION AVAILABLE"` - No info available
- `"89,NOT A DISK IMAGE"` - File is not a valid disk image
- `"90,DRIVE NOT PRESENT"` - Drive ID not available
- `"91,INCOMPATIBLE IMAGE"` - Image type incompatible with drive
- `"98,FUNCTION PROHIBITED"` - Function not allowed
- `"99,FUNCTION NOT IMPLEMENTED"` - Feature not implemented

### Notes:
- All multi-byte integers use **little-endian** format.
- Input strings in command messages are **null-terminated**.
- Output strings in reply messages (such as filenames and paths) are usually **not null-terminated**; the packet ends with the last character of the string, and the client must determine the string's length from the message packet length.
- Only one file can be open at a time (opening a new file closes the previous one).
- **FatFS Error Caveat**: Commands operating directly on files/directories (like `DOS_CMD_OPEN_FILE`, `DOS_CMD_DELETE_FILE`, and `DOS_CMD_RENAME_FILE`) return raw FatFS error strings (e.g. `"FILE DOESN'T EXIST"`, `"PATH DOESN'T EXIST"`, `"WRITE PROTECTED"`) on failure, which do not have the standard `"NN,"` numeric code prefix.
- **Empty Success Status**: Some commands (like `DOS_CMD_READ_DATA`) return an empty status message (length 0) on success instead of `"00,OK"`.


You can find the complete implementation in [software/filemanager/dos.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/filemanager/dos.cc), [software/filemanager/dos.h](https://github.com/GideonZ/1541ultimate/blob/master/software/filemanager/dos.h), and [software/filesystem/fs_errors_flags.h](https://github.com/GideonZ/1541ultimate/blob/master/software/filesystem/fs_errors_flags.h).

Based on the code I've found, here's the complete message structure for each UCI network command:

## UCI Network Command Message Structures

### Command Structures:

#### **0x01 - NET_CMD_IDENTIFY**
```
Command:   [0x03, 0x01]
Length:   2 bytes
Reply:    "ULTIMATE-II NETWORK INTERFACE V1.0" (34 bytes)
Status:   "00,OK"
```

#### **0x02 - NET_CMD_GET_INTERFACE_COUNT**
```
Command:  [0x03, 0x02]
Length:   2 bytes
Reply:    [count(1 byte), name1\0, name2\0, ...] (count followed by null-terminated interface name strings)
Status:   "00,OK"
```

#### **0x03 - NET_CMD_SET_INTERFACE** *(commented out in firmware)*
```
Command:  [0x03, 0x03, interface_number]
Length:   3 bytes
Reply:    Empty
Status:   "00,OK" or error
```

#### **0x04 - NET_CMD_GET_NETADDR** (Get MAC Address)
```
Command:  [0x03, 0x04, interface_number]
Length:   3 bytes
Reply:    [mac0, mac1, mac2, mac3, mac4, mac5] (6 bytes)
Status:   "00,OK" or error
```

#### **0x05 - NET_CMD_GET_IPADDR**
```
Command:  [0x03, 0x05, interface_number]
Length:   3 bytes
Reply:    [IP address, netmask, gateway] (12 bytes total - 4 bytes each)
Status:   "00,OK" or error
```

#### **0x06 - NET_CMD_SET_IPADDR**
```
Command:  [0x03, 0x06, interface_number, ip0, ip1, ip2, ip3,
           mask0, mask1, mask2, mask3, gw0, gw1, gw2, gw3]
Length:   15 bytes (2 header + 1 interface + 12 IP data)
Reply:    Empty
Status:    "00,OK" or error
```

#### **0x07 - NET_CMD_OPEN_TCP**
```
Command:  [0x03, 0x07, port_low, port_high, hostname...]
Length:   5+ bytes (minimum: 2 header + 2 port + 1+ hostname)
          - port_low:  Low byte of port number
          - port_high: High byte of port number
          - hostname: Null-terminated hostname or IP string
Reply:    [socket_number] (1 byte, 0-255)
Status:   "00,OK" or error codes:
          - "84,UNRESOLVED HOST"
          - "85,ERROR OPENING SOCKET"
          - "11,ERROR ON CONNECT: <errno>"
```

#### **0x08 - NET_CMD_OPEN_UDP**
```
Command:  [0x03, 0x08, port_low, port_high, hostname...]
Length:   5+ bytes (same structure as OPEN_TCP)
Reply:    [socket_number] (1 byte, 0-255)
Status:   Same as OPEN_TCP
```

#### **0x09 - NET_CMD_CLOSE_SOCKET**
```
Command:  [0x03, 0x09, socket_number]
Length:   3 bytes
Reply:    Empty
Status:   "00,OK" or "12,ERROR ON CLOSE: <errno>"
```

#### **0x10 - NET_CMD_READ_SOCKET**
```
Command:  [0x03, 0x10, socket_number, length_low, length_high]
Length:    5 bytes
          - socket_number: Socket to read from (0-255)
          - length_low: Low byte of bytes to read
          - length_high: High byte of bytes to read
          - Max read length: CMD_MAX_REPLY_LEN - 2 (894 bytes)
Reply:    [bytes_read_low, bytes_read_high, data...]
          - First 2 bytes: Number of bytes actually read (little-endian)
          - Remaining bytes: The actual data
Status:   "00,OK" or error codes:
          - "01,CONNECTION CLOSED BY HOST"
          - "02,NO DATA: <errno>"
```

#### **0x11 - NET_CMD_WRITE_SOCKET**
```
Command:  [0x03, 0x11, socket_number, data...]
Length:   3+ bytes
          - socket_number: Socket to write to
          - data:  Bytes to send (length = command->length - 3)
Reply:    [bytes_written_low, bytes_written_high]
          - Number of bytes actually written (little-endian)
Status:   "00,OK" or "12,SEND ERROR: <errno>"
```

### Common Error Status Messages:
- `"00,OK"` - Success
- `"81,INVALID PARAMS"` - Invalid parameter count
- `"82,PARAMETER(S) OUT OF RANGE"` - Parameter value out of range
- `"83,INTERFACE NOT AVAILABLE"` - Network interface not available
- `"86,INTERNAL ERROR"` - Internal error occurred

### Client-Side Success Rule (Important)

This rule applies to all targets that use text status format (`"NN,..."`), including Network, DOS, and most Control commands.

- Treat command success as both: command transport succeeded and status code is `00`.
- Do not rely only on the presence of reply data (for example, a socket number byte).
- Real-world symptom: `OPEN_TCP` may return a reply byte while status indicates failure such as `"84,UNRESOLVED HOST"`; a later `WRITE_SOCKET` then fails with `"12,SEND ERROR: <errno>"`.




SoftIEC caveat:

- Several SoftIEC commands use binary one-byte status values instead of text (`"NN,..."`).
- For those commands, apply the command-specific binary status semantics rather than text parsing.

### Notes:
- All multi-byte integers use **little-endian** format (low byte first)
- Maximum command length:  896 bytes
- Maximum reply length: 896 bytes
- Hostnames in OPEN_TCP/UDP are null-terminated strings
- Socket numbers are managed in an internal fixed-size array, typical range is much smaller than 0-255.
- **No loopback support**: The firmware compiles lwIP with `LWIP_HAVE_LOOPIF=0` and `LWIP_NETIF_LOOPBACK=0` (see `software/network/config/lwipopts.h`). This means:
  - `127.0.0.1` does not exist - there is no loopback interface.
  - Connecting to the device's own IP address from the C64 is not supported; the connect call will fail with `errno 113` (EHOSTUNREACH) or `118` (ENETUNREACH).
  - Outbound TCP/UDP connections must target a **different host** on the network.

You can find the complete implementation in [software/io/network/network_target.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/io/network/network_target.cc) and [software/io/network/network_target.h](https://github.com/GideonZ/1541ultimate/blob/master/software/io/network/network_target.h).


## UCI Control Command Message Structures

All control commands target `UCI_TARGET_CONTROL` (`0x04`).

### Command Structures:

#### **0x01 - CTRL_CMD_IDENTIFY**
```
Command:  [0x04, 0x01]
Length:   2 bytes
Reply:    "CONTROL TARGET V1.1" (19 bytes)
Status:   "00,OK"
Notes:    Returns the control target subsystem identification string.
          Use CTRL_CMD_GET_HWINFO device=0 for the actual hardware product name.
```

#### **0x02 - CTRL_CMD_READ_RTC**
```
Status:   Always "21,UNKNOWN COMMAND".
Notes:    This command ID is defined in the firmware header but has no case
          handler in the switch statement. It is effectively dead code.
          Use DOS_CMD_GET_TIME (0x26) for RTC access instead.
```

#### **0x03 - CTRL_CMD_FINISH_CAPTURE**
```
Command:  [0x04, 0x03]
Length:   2 bytes
Reply:    Empty
Status:   "00,OK"
Notes:    Finishes an active tape/audio capture session.
          Dispatches to SUBSYSID_TAPE_RECORDER, MENU_REC_FINISH.
```

#### **0x05 - CTRL_CMD_FREEZE**
```
Command:  [0x04, 0x05]
Length:   2 bytes
Reply:    Empty
Status:   Empty (0-length status)
Notes:    Simulates pressing the physical menu button on the Ultimate cartridge.
          Freezes the C64 and brings up the Ultimate settings / menu screen.
```

#### **0x06 - CTRL_CMD_REBOOT**
```
Command:  [0x04, 0x06]
Length:   2 bytes
Reply:    Empty
Status:   Empty (0-length status)
Notes:    Restarts the C64 with the current cartridge configuration.
          Executes MENU_C64_REBOOT (c64->start_cartridge(NULL)) which
          reinitializes the C64 FPGA core. The ARM firmware keeps running.
          This is NOT the same as a C64 reset (see REST API machine:reset).
```

#### **0x08 - CTRL_CMD_LOAD_REU**
```
Command:  [0x04, 0x08, dummy0, dummy1, dummy2]
Length:   5+ bytes
Reply:    [retVal(4), status_string...]
          - retVal: 32-bit little-endian return value (positive/success = length of status string, negative = error)
          - status_string: Loaded REU image file path status (text string)
Status:   "00,OK" or error codes:
          - "81,INVALID PARAMS" (command length < 5)
          - "84,REU NOT ENABLED"
          - "85,REU FILE CANNOT BE OPENED"
Notes:    Loads the REU image file configured in cartridge settings into REU memory. The command must be at least 5 bytes long, so at least 3 dummy bytes must be appended to the command packet.
```

#### **0x09 - CTRL_CMD_SAVE_REU**
```
Command:  [0x04, 0x09, dummy0, dummy1, dummy2]
Length:   5+ bytes
Reply:    [retVal(4), status_string...] (same layout as CTRL_CMD_LOAD_REU)
Status:   "00,OK" or error codes:
          - "81,INVALID PARAMS" (command length < 5)
          - "84,REU NOT ENABLED"
          - "85,REU FILE CANNOT BE OPENED"
          - "86,REU OFFSET > SIZE. NOT SAVED"
Notes:    Saves REU memory back to the configured default REU image file. Same length constraint as LOAD_REU.
```

#### **0x0F - CTRL_CMD_U64_SAVEMEM** (Ultimate 64 only)
```
Command:  [0x04, 0x0F, filename...]
Length:   2+ bytes
          - filename: Optional null-terminated destination path.
                      Defaults to "/temp/c64_memory.bin" if omitted.
Reply:    Empty
Status:   "00,OK" or "87,DISK ERR: <fatfs_error>"
Notes:    Saves the entire 64 KiB C64 memory space to a file using DMA.
          Only available on Ultimate 64 (#ifdef U64 in firmware).
```

#### **0x11 - CTRL_CMD_DECODE_TRACK**
```
Command:  [0x04, 0x11, track, max_sector, gcr_addr(3), bin_addr(3), track_len(2)]
Length:   14 bytes (exact)
          - track:       Track number to decode
          - max_sector:  Maximum number of sectors expected
          - gcr_addr:    24-bit GCR source address (3 bytes, little-endian)
          - bin_addr:    24-bit binary destination address (3 bytes, little-endian)
          - track_len:   GCR track data length (2 bytes, little-endian)
Reply:    [sector_count(1), (error_code, spare)*N...]
          - sector_count: Number of sectors decoded
          - For each sector: 2 bytes (error code + spare)
Status:   "00,OK" or "82,ERRORS ON TRACK" (if any sector had errors)
Notes:    Converts GCR-encoded track data to binary sectors at the given
          memory addresses. Returns per-sector error codes in the reply.
```

#### **0x12 - CTRL_CMD_ENCODE_TRACK**
```
Command:  [0x04, 0x12, ...]
Status:   Defined in firmware header (control_target.h) but the switch
          statement has no case handler. Falls through to "21,UNKNOWN COMMAND".
Notes:    Intended as the inverse of DECODE_TRACK (binary -> GCR).
          Not currently implemented.
```

#### **0x20 - CTRL_CMD_EASYFLASH**
```
Command:  [0x04, 0x20, subcommand, ...]
Length:   3+ bytes
          - subcommand: 0 = sector erase (only subcommand implemented)
Reply:    Empty
Status:   "00,OK" or "81,INVALID PARAMS"
Notes:    EasyFlash cartridge in-system programming. Subcommand 0 erases
          an EasyFlash chip sector: clears 8 banks × 8 KiB to 0xFF.
          Command format for subcommand 0:
            [0x04, 0x20, 0x00, bank_info, base_addr]
            - bank_info: bits 3-5 select the bank (0-7), masked with 0x38
            - base_addr: bit 5 set = high ROM ($A000), clear = low ROM ($8000)

          **IMPORTANT**: This does NOT load .crt files. For CRT loading, use
          the REST API (/v1/runners:run_crt) or raw socket DMA (SOCKET_CMD_RUN_CRT).
```

#### **0x28 - CTRL_CMD_GET_HWINFO**
```
Command:  [0x04, 0x28, device]
Length:   2 or 3 bytes
          - device: 0 for product string, 1 for SID configuration
Reply:    Product name string (device 0) or hardware SID base/mask parameters (device 1)
Status:   "00,OK" or error
```

#### **0x29 - CTRL_CMD_GET_DRVINFO**
```
Command:  [0x04, 0x29, effective_id]
Length:   2 or 3 bytes
          - effective_id: 1 to get effective IEC address, 0 to get current
Reply:    [drives_count, (drive_type, iec_address, power_state)*N, ...]
          - drives_count: Total number of drive + IEC slave devices
          - Per virtual drive (A/B): 3 bytes - type, iec_addr, power (0/1)
          - Per IEC slave:           3 bytes - type, addr, enabled (0/1)
          The reply is variable-length; iterate based on drives_count.
Status:   "00,OK"
```

#### **0x30 - CTRL_CMD_ENABLE_DISK_A**
```
Command:  [0x04, 0x30]
Length:   2 bytes
Reply:    Empty
Status:   "00,OK"
Notes:    Enables virtual drive A.
```

#### **0x31 - CTRL_CMD_DISABLE_DISK_A**
```
Command:  [0x04, 0x31]
Length:   2 bytes
Reply:    Empty
Status:   "00,OK"
Notes:    Disables virtual drive A.
```

#### **0x32 - CTRL_CMD_ENABLE_DISK_B**
```
Command:  [0x04, 0x32]
Length:   2 bytes
Reply:    Empty
Status:   "00,OK"
Notes:    Enables virtual drive B.
```

#### **0x33 - CTRL_CMD_DISABLE_DISK_B**
```
Command:  [0x04, 0x33]
Length:   2 bytes
Reply:    Empty
Status:   "00,OK"
Notes:    Disables virtual drive B.
```

#### **0x34 - CTRL_CMD_DISK_A_POWER**
```
Command:  [0x04, 0x34]
Length:   2 bytes
Reply:    "off" or "on " (3-byte text string)
Status:   "00,OK"
Notes:    Read-only query command. To change power, use CTRL_CMD_ENABLE_DISK_A / DISABLE_DISK_A.
```

#### **0x35 - CTRL_CMD_DISK_B_POWER**
```
Command:  [0x04, 0x35]
Length:   2 bytes
Reply:    "off" or "on " (3-byte text string)
Status:   "00,OK"
Notes:    Read-only query command. To change power, use CTRL_CMD_ENABLE_DISK_B / DISABLE_DISK_B.
```

#### **0x40 - CTRL_CMD_GET_RAMDISKINFO**
```
Command:  [0x04, 0x40]
Length:   2 bytes
Reply:    [type, size...] (8 bytes total representing the layout of the 4 RAM disks)
Status:   "00,OK"
```

You can find the control target implementation in [software/io/command_interface/control_target.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/io/command_interface/control_target.cc) and [software/io/command_interface/control_target.h](https://github.com/GideonZ/1541ultimate/blob/master/software/io/command_interface/control_target.h).


## UCI SoftIEC Target (Target 0x05)

The SoftIEC target registers to target ID `0x05` to handle KERNAL-level intercepts and serial bus operations.

### Status Codes
SoftIEC commands return single-byte binary status codes in the status buffer, rather than human-readable status strings (except where noted).
- `\x00` - OK / Success
- `\x01` - File not found
- `\x02` - Save error
- `\x03` - No input channel
- `\x04` - Unknown command
- `\x05` - IEC module not loaded

### Command Structures:

#### **0x01 - SOFTIEC_CMD_IDENTIFY**
```
Command:  [0x05, 0x01]
Length:   2 bytes
Reply:    "SOFTWARE IEC TARGET V1.0" (24 bytes)
Status:   "00,OK" (string format)
```

#### **0x10 - SOFTIEC_CMD_LOAD_SU**
```
Command:  [0x05, 0x10, secondary_addr, verify_flag, load_addr_lo, load_addr_hi, end_addr_lo, end_addr_hi, filename...]
Length:   8+ bytes (filename must be null-terminated)
Reply:    [start_address(2)] (little-endian start address of the file)
Status:   \x00 (binary OK) or \x01 (binary File not found)
Notes:    Prepares a file load from a virtual drive. If secondary_addr is non-zero, the load_addr passed in command is overridden by the file's native load address.
          This command only opens the file and returns its start address. The actual data must be loaded using SOFTIEC_CMD_LOAD_EX.
```

#### **0x11 - SOFTIEC_CMD_LOAD_EX**
```
Command:  [0x05, 0x11, secondary_addr, verify_flag]
Length:   4 bytes
Reply:    Empty
Status:   If verify_flag is non-zero: [0x80] on verify error, or [0x00] on success.
          If verify_flag is zero: [0x00, end_addr_lo, end_addr_hi] on success.
Notes:    Executes the actual file load/verify operation on the channel prepared by SOFTIEC_CMD_LOAD_SU.
```

#### **0x12 - SOFTIEC_CMD_SAVE**
```
Command:  [0x05, 0x12, verify_flag, secondary_addr, start_addr_lo, start_addr_hi, end_addr_lo, end_addr_hi, filename...]
Length:   8+ bytes
Reply:    Empty
Status:   \x00 (binary OK) or \x02 (binary Save error)
```
**Caveats & Behavior:**
- **Filesystem Pathing**: SoftIEC commands operate directly on the host FAT partition (e.g. `/usb0/`) by default. They do *not* automatically follow emulated drive mounts (like disk images mounted on drive 8) unless the SoftIEC partition has been CD'd into the D64 image first using the command channel (secondary address 15) with `CD:<path_to_d64>`.
- **Filename Case Sensitivity**: When writing to the host FAT partition, uppercase PETSCII filenames are converted to lowercase ASCII filenames (e.g., `"UCITEST"` becomes `"ucitest.prg"`).
- **File Overwriting**: If the destination file already exists, the save operation will fail with a Save error (`\x02`), representing `FILE EXISTS` (FatFs `FR_EXIST`). To overwrite an existing file, prefix the filename with `@` (e.g. `"@UCITEST"`), which is the standard Commodore replace syntax. This directs the firmware to open the file with write-and-replace flags (`FA_WRITE | FA_CREATE_ALWAYS`).


#### **0x13 - SOFTIEC_CMD_OPEN**
```
Command:  [0x05, 0x13, secondary_addr, unused, filename...]
Length:   4+ bytes
Reply:    Empty
Status:   Empty (0-length status)
```

#### **0x14 - SOFTIEC_CMD_CLOSE**
```
Command:  [0x05, 0x14, secondary_addr, unused]
Length:   4 bytes
Reply:    Empty
Status:   "00,OK" (string format)
```

#### **0x15 - SOFTIEC_CMD_CHKIN**
```
Command:  [0x05, 0x15, secondary_addr, unused]
Length:   4 bytes
Reply:    [prefetched_bytes...] (up to 32 bytes from channel)
Status:   Empty (0-length status)
Notes:    Prepares an input channel for reading. The C64 should read the prefetched reply data, then write CTL_DATA_ACC (0x02) to the control register to request further chunks.
```

#### **0x16 - SOFTIEC_CMD_CHKOUT**
```
Command:  [0x05, 0x16, secondary_addr, unused, data...]
Length:   4+ bytes
Reply:    Empty
Status:   "00,OK" (string format)
Notes:    If secondary_addr has the high nibble set to 0xF0 (0xF0–0xFF), it behaves as an OPEN command.
          If secondary_addr has the high nibble set to 0xE0 (0xE0–0xEF), it behaves as a CLOSE command.
          Otherwise, it writes the payload data to the channel.
```

#### **0x1A - SOFTIEC_CMD_LOAD_MOUNT_SU** / **0x1B - SOFTIEC_CMD_LOAD_MOUNT_EX**
```
Command:  [0x05, 0x1A or 0x1B, ...]
Notes:    Identical layout and behavior to SOFTIEC_CMD_LOAD_SU / LOAD_EX, but also mounts the containing disk image automatically.
```

You can find the SoftIEC target implementation in [software/io/command_interface/softiec_target.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/io/command_interface/softiec_target.cc) and [software/io/command_interface/softiec_target.h](https://github.com/GideonZ/1541ultimate/blob/master/software/io/command_interface/softiec_target.h).

---

### **Concurrency & Lockup Caveats (FreeRTOS Lock Inversion)**

A key issue in hardware-in-the-loop testing and heavy SoftIEC usage is random C64/cartridge hangs. The architectural root cause is a **Lock Inversion Deadlock** in the Ultimate cartridge's multi-threaded FreeRTOS environment:

1. **Shared Filesystem Locks**: Operations involving host directory listing (`$`), file opening, saving, or mounting require acquiring the global `FileManager` mutex to perform FAT filesystem reads/writes on the physical SD card or USB drive.
2. **Lock Acquisition Block**: If another background task (such as the cartridge UI menu thread, the emulated 1541 floppy drive thread, or the web server REST API thread writing data to USB) holds this mutex, the Command Interface thread suspends waiting for the lock.
3. **Bus Hold State**: While the Command Interface thread is suspended on the FreeRTOS recursive semaphore (`xSemaphoreTakeRecursive(serializer, portMAX_DELAY)`), the command processing loop remains blocked, leaving the Command Interface status register in the `STAT_STATE_BUSY` state.
4. **C64 Spin-lock**: The Commodore 64 CPU polls the status register in a tight loop waiting for `STAT_STATE_BUSY` to clear. Because the C64 CPU is stuck in this loop, it cannot process interrupts or floppy/serial bus handshakes.
5. **Web Server Responsiveness**: Since the web server and the machine controller run in separate FreeRTOS threads and have their interrupts correctly masked/managed (preventing ISR storms), the HTTP API remains active and responsive (e.g. `machine:readmem` calls still succeed) even while the C64 CPU and Command Interface thread are deadlocked.
6. **Recovery**: Once a deadlock occurs, the C64 and the cartridge emulator are completely locked. The only way to recover is to perform a hard physical power cycle on the C64 machine.
7. **Mitigation**: Introducing a brief delay (e.g., 50ms–100ms) in test harnesses after host-side filesystem operations (like uploading a PRG or creating/mounting a D64) before running the C64 program ensures that any pending write/sync filesystem locks are fully released before the C64 attempts to access filesystem-backed targets.
