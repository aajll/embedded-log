# embedded-log

[![CI](https://github.com/aajll/embedded-log/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/embedded-log/actions/workflows/ci.yml)

A lightweight, MISRA C:2023 aware RAM log buffer for embedded systems without traditional stdout, UART, or file-based logging interfaces. Log entries are stored in a caller-owned static ring buffer and can be inspected live via JTAG or during post-mortem debugging with no dynamic memory or OS dependencies.

## Features

- **RAM ring buffer** — fixed-size static storage in a caller-owned
  `struct log_ctx`; no `malloc` / `free`.
- **Log levels** — `LOG_INFO`, `LOG_WARN`, `LOG_FAULT`.
- **`printf`-style API** — `log_event(ctx, level, fmt, ...)`.
- **`LOG_ONCE`** — record a message at most once per code location per reset,
  preventing log spam in state machines and tight loops.
- **Defensive** — safe against NULL pointers and misuse.
- **MISRA C:2023 aware, Linux kernel style** — Doxygen-annotated, suitable
  for resource-constrained microcontrollers.
- **Meson subproject ready** — integrates as a wrap dependency.

## Installation

### Copy-in (recommended for embedded targets)

Copy the following files into your project tree; no build system required:

```
include/log.h
include/log_conf.h
src/log.c
```

Place them in your include path, then include the header:

```c
#include "log.h"
```

### Meson subproject

Add this repo as a wrap dependency or subproject. The library exposes an `embedded_log_dep` dependency object that carries the correct include path:

```meson
embedded_log_dep = dependency('embedded-log', fallback : ['embedded-log', 'embedded_log_dep'])
```

## Quick Start

```c
#include <stdio.h>
#include "log.h"

static struct log_ctx app_log;

/* You must supply a monotonic timestamp source. The unit (milliseconds,
 * ticks, ...) is user-defined. */
static uint32_t my_get_ticks(void)
{
        return 0U; /* replace with your tick / RTC source */
}

void app_init(void)
{
        log_init(&app_log, my_get_ticks);
}

void foo(int error)
{
        log_event(&app_log, LOG_INFO, "Entering foo()");
        if (error != 0) {
                log_event(&app_log, LOG_FAULT, "Error: %d", error);
        }
        LOG_ONCE(&app_log, LOG_WARN, "This warning is recorded once per reset.");
}

void dump_log(void)
{
        for (uint16_t i = 0U; i < log_get_count(&app_log); ++i) {
                const struct log_entry *e = log_get_entry(&app_log, i);
                printf("[%u] %u: %s\n",
                       (unsigned)e->timestamp, (unsigned)e->level, e->msg);
        }
}
```

## Configuration

The buffer geometry is set by macros in `log_conf.h` (included automatically by `log.h`). Override them before including the header or pass them as `-D` flags on the compiler command line.

| Macro         | Description                                                     | Default |
| ------------- | --------------------------------------------------------------- | ------- |
| `LOG_ENTRIES` | Number of entries retained in the ring buffer.                  | `64U`   |
| `LOG_MSG_LEN` | Maximum formatted message length, including the NUL terminator. | `64U`   |

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

## API Reference

### Core Functions

```c
void log_init(struct log_ctx *ctx, uint32_t (*timestamp_fn)(void));
void log_event(struct log_ctx *ctx, enum log_level level, const char *fmt, ...);
```

`log_init` initialises a caller-owned context with a user-supplied monotonic timestamp function. `log_event` records a `printf`-style formatted entry at the given level (`LOG_INFO`, `LOG_WARN`, or `LOG_FAULT`); the message is truncated to `LOG_MSG_LEN - 1` characters and is always NUL-terminated.

### One-Shot Logging

```c
LOG_ONCE(ctx, level, ...);
```

Records an entry at most once per code location per reset. The guard flag lives in function-local static storage at the expansion site, so each call site is tracked independently.

### Inspection Functions

```c
uint16_t log_get_count(const struct log_ctx *ctx);
const struct log_entry *log_get_entry(const struct log_ctx *ctx, uint16_t idx);
const struct log_entry *log_get_buffer(const struct log_ctx *ctx,
                                       uint16_t *count);
```

`log_get_count` returns the number of valid entries currently stored. `log_get_entry` returns the `idx`-th oldest entry (`0` = oldest), or `NULL` if out of bounds. `log_get_buffer` returns a pointer to the underlying entry array for direct inspection and, if `count` is non-NULL, writes the number of valid entries; the array is in physical (storage) order, which differs from chronological order once the ring has wrapped. Use `log_get_entry` for oldest-first access.

### Log Entry

```c
struct log_entry {
        uint32_t timestamp;
        uint16_t level;
        char msg[LOG_MSG_LEN];
};
```

## Notes

| Topic                | Note                                                                                                                                                                                                              |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Memory**           | All storage lives in the caller-owned `struct log_ctx`. Verify `LOG_ENTRIES` × `LOG_MSG_LEN` fits your RAM budget.                                                                                                |
| **Timestamp source** | `log_init` requires a monotonic timestamp function; the unit is user-defined.                                                                                                                                     |
| **Thread safety**    | Not internally synchronised. Protect a shared context with an external mutex or confine it to a single execution context.                                                                                         |
| **NULL safety**      | Public functions are defensive against NULL pointers and return without effect rather than faulting.                                                                                                              |
| **MISRA C:2023**     | Aware, not formally compliant. Accepted deviations: Rule 17.1 (`stdarg.h`) and Rule 21.6 (`stdio.h`), required by the `printf`-style `log_event` API. `vsnprintf` is bounded and output is always NUL-terminated. |
