# embedded-log

[![CI](https://github.com/aajll/embedded-log/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/embedded-log/actions/workflows/ci.yml)

A lightweight, MISRA C:2023 aware RAM log buffer for embedded systems without traditional stdout, UART, or file-based logging interfaces. Log entries are stored in a caller-owned static ring buffer and can be inspected live via JTAG or during post-mortem debugging with no dynamic memory or OS dependencies.

## Features

- **RAM ring buffer** — fixed-size static storage in a caller-owned `struct emlog_ctx`; no `malloc` / `free`.
- **Log levels** — `EMLOG_INFO`, `EMLOG_WARN`, `EMLOG_FAULT`.
- **`printf`-style API** — `emlog_event(ctx, level, fmt, ...)`.
- **`EMLOG_ONCE`** — record a message at most once per code location per reset, preventing log spam in state machines and tight loops.
- **Interrupt-safe deferred path** — `emlog_event_isr()` records a format and two `uint32_t` arguments lock-free from an interrupt; `emlog_drain()` formats them later in the background.
- **Prefixed names** — every identifier starts with `emlog_` or `EMLOG_`, so the header can be included alongside `<syslog.h>`.
- **Defensive** — safe against NULL pointers and misuse.
- **MISRA C:2023 aware, Linux kernel style** — Doxygen-annotated, suitable for resource-constrained microcontrollers.
- **Meson subproject ready** — integrates as a wrap dependency.

## Installation

### Copy-in (recommended for embedded targets)

Copy the following files into your project tree; no build system required:

```
include/emlog.h
include/emlog_conf.h
src/emlog.c
```

Place them in your include path, then include the header:

```c
#include "emlog.h"
```

### Meson subproject

Add this repo as a wrap dependency or subproject. The library exposes an `embedded_log_dep` dependency object that carries the correct include path:

```meson
embedded_log_dep = dependency('embedded-log', fallback : ['embedded-log', 'embedded_log_dep'])
```

## Quick Start

```c
#include <stdio.h>
#include "emlog.h"

static struct emlog_ctx app_log;

/* You must supply a monotonic timestamp source. The unit (milliseconds,
 * ticks, ...) is user-defined. */
static uint32_t my_get_ticks(void)
{
        return 0U; /* replace with your tick / RTC source */
}

void app_init(void)
{
        emlog_init(&app_log, my_get_ticks);
}

void foo(int error)
{
        emlog_event(&app_log, EMLOG_INFO, "Entering foo()");
        if (error != 0) {
                emlog_event(&app_log, EMLOG_FAULT, "Error: %d", error);
        }
        EMLOG_ONCE(&app_log, EMLOG_WARN, "This warning is recorded once per reset.");
}

void dump_log(void)
{
        for (uint16_t i = 0U; i < emlog_get_count(&app_log); ++i) {
                const struct emlog_entry *e = emlog_get_entry(&app_log, i);
                printf("[%lu] %u: %s\n",
                       (unsigned long)e->timestamp, (unsigned)e->level, e->msg);
        }
}
```

## Configuration

The buffer geometry is set by macros in `emlog_conf.h` (included automatically by `emlog.h`). Override them before including the header or pass them as `-D` flags on the compiler command line. The library and every translation unit that includes the header must see the same values.

| Macro           | Description                                                     | Default |
| --------------- | --------------------------------------------------------------- | ------- |
| `EMLOG_ENTRIES` | Number of entries retained in the ring buffer.                  | `64U`   |
| `EMLOG_MSG_LEN` | Maximum formatted message length, including the NUL terminator. | `64U`   |
| `EMLOG_ISR_ENTRIES` | Entries `emlog_event_isr()` can hold until the next `emlog_drain()`, from 1 to 254. | `8U` |

The interrupt path picks an atomic backend automatically: GCC/Clang `__atomic` builtins, then C11 `<stdatomic.h>`, then `volatile`. Define one of `EMLOG_USE_GNU_ATOMICS`, `EMLOG_USE_C11_ATOMICS` or `EMLOG_USE_NO_ATOMICS` to choose it explicitly, identically for the library and its callers. The `volatile` backend is correct on a single core where the interrupt pre-empts the draining context; on a multi-core target without atomics, define `EMLOG_FENCE_ACQUIRE()` and `EMLOG_FENCE_RELEASE()` as the hardware barriers.

## Migrating from `log.h`

`log.h`, `log_conf.h` and `log_version.h` remain as deprecated forwarding headers, so existing code compiles unchanged. They map each original name onto its prefixed form with a macro, and a `LOG_ENTRIES` or `LOG_MSG_LEN` override still sets the matching `EMLOG_` value. They will be removed in the next major release.

| Deprecated | Replacement |
| --- | --- |
| `#include "log.h"` | `#include "emlog.h"` |
| `struct log_ctx`, `struct log_entry`, `enum log_level` | `struct emlog_ctx`, `struct emlog_entry`, `enum emlog_level` |
| `log_init`, `log_event`, `log_get_count`, `log_get_entry`, `log_get_buffer` | `emlog_init`, `emlog_event`, `emlog_get_count`, `emlog_get_entry`, `emlog_get_buffer` |
| `LOG_INFO`, `LOG_WARN`, `LOG_FAULT`, `LOG_ONCE`, `LOG_PRINTF_LIKE` | `EMLOG_INFO`, `EMLOG_WARN`, `EMLOG_FAULT`, `EMLOG_ONCE`, `EMLOG_PRINTF_LIKE` |
| `LOG_ENTRIES`, `LOG_MSG_LEN`, `LOG_VERSION*` | `EMLOG_ENTRIES`, `EMLOG_MSG_LEN`, `EMLOG_VERSION*` |

Because the forwarders are object-like macros, `log.h` renames those identifiers throughout any file that includes it, and it still collides with `<syslog.h>`.

## Building

```sh
# Library only (release)
meson setup build --buildtype=release
meson compile -C build

# With unit tests
meson setup build --buildtype=debug -Dbuild_tests=true
meson compile -C build
meson test -C build
```

Tests are disabled by default; enable them with `-Dbuild_tests=true` at setup time.

## Platform support

The library is portable C11 and needs only `<stdbool.h>`, `<stdint.h>`, `<stdarg.h>`, `<stdio.h>` and `<string.h>`, plus `<stdatomic.h>` when the C11 backend is selected.

| Toolchain                       | Target                | Status                   |
| ------------------------------- | --------------------- | ------------------------ |
| GCC, Clang                      | x86_64 Linux          | Host tests (CI)          |
| TI C2000 CGT 22.6 and 25.11 LTS | 16-bit-MAU DSP family | Library cross-build (CI) |

CI cross-compiles the library with warnings as errors under both the COFF and EABI object formats. That toolchain has no C11 atomics, so it uses the `volatile` backend. The `EMLOG_ONCE` guard is `uint_least8_t`, so the macro also compiles where `CHAR_BIT == 16`.

## API Reference

### Core Functions

```c
void emlog_init(struct emlog_ctx *ctx, uint32_t (*timestamp_fn)(void));
void emlog_event(struct emlog_ctx *ctx, enum emlog_level level, const char *fmt, ...);
```

`emlog_init` initialises a caller-owned context with a user-supplied monotonic timestamp function. `emlog_event` records a `printf`-style formatted entry at the given level (`EMLOG_INFO`, `EMLOG_WARN`, or `EMLOG_FAULT`); the message is truncated to `EMLOG_MSG_LEN - 1` characters and is always NUL-terminated.

### Logging from an interrupt

```c
bool emlog_event_isr(struct emlog_ctx *ctx, enum emlog_level level, const char *fmt, uint32_t arg0, uint32_t arg1);
uint16_t emlog_drain(struct emlog_ctx *ctx);
uint_least8_t emlog_get_isr_dropped(const struct emlog_ctx *ctx);
```

`emlog_event_isr` captures the timestamp, level, format pointer and both arguments into a pending ring without formatting, locking or blocking, and returns `false` if the ring is full or an argument is invalid. `emlog_drain` formats the pending entries into the log in posting order, keeping the timestamps captured at posting, and returns how many it moved. `emlog_get_isr_dropped` counts entries rejected because the pending ring was full, saturating at `UINT_LEAST8_MAX`; `emlog_init` resets it.

```c
void adc_isr(void)
{
        (void)emlog_event_isr(&log_ctx, EMLOG_FAULT, "adc %" PRIu32 " ch %" PRIu32, sample, channel);
}

void background_loop(void)
{
        (void)emlog_drain(&log_ctx);
}
```

The context rules are:

- `emlog_event`, `emlog_drain`, `EMLOG_ONCE` and the inspection functions run in one execution context, never concurrently with each other.
- `emlog_event_isr` runs from one producer context at a time and may pre-empt that context at any point. Interrupts of different priorities that both log must not pre-empt each other inside the call.
- The timestamp function must be safe to call from the producer context.
- `fmt` must have static storage duration, such as a string literal, because it is read when the entry is drained. Its conversions must consume `uint32_t` arguments (`PRIu32`, `PRIx32`); unused arguments are ignored.

### One-Shot Logging

```c
EMLOG_ONCE(ctx, level, ...);
```

Records an entry at most once per code location per reset. The guard flag lives in function-local static storage at the expansion site, so each call site is tracked independently.

### Inspection Functions

```c
uint16_t emlog_get_count(const struct emlog_ctx *ctx);
const struct emlog_entry *emlog_get_entry(const struct emlog_ctx *ctx, uint16_t idx);
const struct emlog_entry *emlog_get_buffer(const struct emlog_ctx *ctx, uint16_t *count);
```

`emlog_get_count` returns the number of valid entries currently stored. `emlog_get_entry` returns the `idx`-th oldest entry (`0` = oldest), or `NULL` if out of bounds. `emlog_get_buffer` returns a pointer to the underlying entry array for direct inspection and, if `count` is non-NULL, writes the number of valid entries; the array is in physical (storage) order, which differs from chronological order once the ring has wrapped. Use `emlog_get_entry` for oldest-first access.

### Log Entry

```c
struct emlog_entry {
        uint32_t timestamp;
        uint16_t level;
        char msg[EMLOG_MSG_LEN];
};
```

## Notes

| Topic                | Note |
| -------------------- | ---- |
| **Memory**           | All storage lives in the caller-owned `struct emlog_ctx`. Verify `EMLOG_ENTRIES` × `EMLOG_MSG_LEN`, plus `EMLOG_ISR_ENTRIES + 1` pending entries, fits your RAM budget. |
| **Timestamp source** | `emlog_init` requires a monotonic timestamp function; the unit is user-defined. |
| **Thread safety**    | Not internally synchronised, except that one producer may call `emlog_event_isr` concurrently with the context that owns the log. Protect other sharing with an external mutex. |
| **NULL safety**      | Public functions are defensive against NULL pointers and return without effect rather than faulting. |
| **Version header**   | `emlog_version.h` is generated at configure time and defines `EMLOG_VERSION` and its components. |
| **MISRA C:2023**     | Aware, not formally compliant. Accepted deviations: Rule 17.1 (`stdarg.h`) and Rule 21.6 (`stdio.h`), required by the `printf`-style `emlog_event` API and the deferred formatting in `emlog_drain`. `vsnprintf` and `snprintf` are bounded and output is always NUL-terminated. |
