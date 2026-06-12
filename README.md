# embedded-log

[![CI](https://github.com/aajll/embedded-log/actions/workflows/ci.yml/badge.svg)](https://github.com/aajll/embedded-log/actions/workflows/ci.yml)

A lightweight, MISRA C:2023 aware RAM log buffer for embedded systems without
traditional stdout, UART, or file-based logging interfaces. Log entries are
stored in a caller-owned static ring buffer and can be inspected live via JTAG
or during post-mortem debugging — with no dynamic memory or OS dependencies.

## Features

- **RAM ring buffer** — fixed-size static storage in a caller-owned
  `struct log_ctx`; no `malloc` / `free`.
- **Log levels** — `INFO`, `WARN`, `FAULT`.
- **`printf`-style API** — `log_event(ctx, level, fmt, ...)`.
- **`LOG_ONCE`** — record a message at most once per code location per reset,
  preventing log spam in state machines and tight loops.
- **Defensive** — safe against NULL pointers and misuse.
- **MISRA C / Linux kernel style** — Doxygen-annotated, suitable for
  resource-constrained microcontrollers.
- **Meson subproject ready** — integrates as a wrap dependency.

## Installation

### Copy-in (recommended for embedded targets)

Copy the following files into your project tree — no build system required:

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

Add this repo as a wrap dependency or subproject. The library exposes an
`embedded_log_dep` dependency object that carries the correct include path:

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
        log_event(&app_log, INFO, "Entering foo()");
        if (error != 0) {
                log_event(&app_log, FAULT, "Error: %d", error);
        }
        LOG_ONCE(&app_log, WARN, "This warning is recorded once per reset.");
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

The buffer geometry is set by macros in `log_conf.h` (included automatically
by `log.h`). Override them before including the header or pass them as `-D`
flags on the compiler command line.

| Macro         | Description                                                     | Default |
| ------------- | --------------------------------------------------------------- | ------- |
| `LOG_ENTRIES` | Number of entries retained in the ring buffer.                  | `50U`   |
| `LOG_MSG_LEN` | Maximum formatted message length, including the NUL terminator. | `48U`   |

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

Pass `-Dbuild_tests=false` at setup time to skip the Unity-based unit tests.

## API Reference

| Function                          | Description                                                                  |
| --------------------------------- | ---------------------------------------------------------------------------- |
| `log_init(ctx, timestamp_fn)`     | Initialise a context with a user-supplied monotonic timestamp function.      |
| `log_event(ctx, level, fmt, ...)` | Record a `printf`-style formatted entry at the given level.                  |
| `LOG_ONCE(ctx, level, fmt, ...)`  | Record an entry at most once per code location per reset.                    |
| `log_get_count(ctx)`              | Return the number of valid entries currently stored.                         |
| `log_get_entry(ctx, idx)`         | Return the `idx`-th oldest entry (`0` = oldest), or `NULL` if out of bounds. |
| `log_get_buffer(ctx, count)`      | Return a pointer to the underlying entry array for direct inspection.        |

## Notes

| Topic                | Note                                                                                                                      |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| **Memory**           | All storage lives in the caller-owned `struct log_ctx`. Verify `LOG_ENTRIES` × `LOG_MSG_LEN` fits your RAM budget.        |
| **Timestamp source** | `log_init` requires a monotonic timestamp function; the unit is user-defined.                                             |
| **Thread safety**    | Not internally synchronised. Protect a shared context with an external mutex or confine it to a single execution context. |
| **NULL safety**      | Public functions are defensive against NULL pointers and return without effect rather than faulting.                      |
