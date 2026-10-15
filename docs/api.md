# API reference

`emlog.h` declares the whole interface and includes `emlog_conf.h`. The examples in this document compile together as one file after `#include "emlog.h"`.

## Conventions

- The context that calls `emlog_event()`, `emlog_drain()`, `EMLOG_ONCE` and the inspection functions owns the log. Those calls never run concurrently with each other; protect any other sharing with your own lock.
- `emlog_event_isr()` is the one exception: one producer context at a time may call it while the owner runs. [Context rules](design.md#context-rules) gives the full set.
- No function allocates, blocks or makes a system call. Every function checks its pointers; a NULL argument never dereferences.

## Context

```c
#include <stdint.h>

static struct emlog_ctx log_ctx;

static uint32_t
board_ticks(void)
{
        return 0u; /* a free-running timer in real firmware */
}

void
log_setup(void)
{
        emlog_init(&log_ctx, board_ticks);
}
```

```c
void emlog_init(struct emlog_ctx *ctx, uint32_t (*timestamp_fn)(void));
```

`struct emlog_ctx` holds the ring of `EMLOG_ENTRIES` formatted entries, the pending ring of the interrupt path, and the timestamp function. Its members are visible so that you can declare it statically and read it from a debugger; only the library should write them.

`emlog_init` empties both rings, zeroes the stored entries, resets the dropped-entry count, and stores `timestamp_fn`. It does nothing if `ctx` is NULL. A NULL `timestamp_fn` is accepted, but every later `emlog_event` and `emlog_event_isr` call then does nothing, so the log stays empty. Call it before any other use of the context, and never while an interrupt may post to it. There is no destroy function, because the context owns no resources.

## Recording

```c
void emlog_event(struct emlog_ctx *ctx, enum emlog_level level, const char *fmt,
                 ...);
```

`emlog_event` takes a timestamp, formats `fmt` and its arguments with `vsnprintf` into the next slot, and stores `level`. When the ring holds `EMLOG_ENTRIES` entries, the new one replaces the oldest. The message is truncated to `EMLOG_MSG_LEN - 1` characters and is always NUL-terminated; if the formatter reports an encoding error, the stored message is empty. It does nothing if `ctx`, `fmt` or the context's timestamp function is NULL.

<!-- snippet: skip -->
```c
enum emlog_level {
        EMLOG_INFO = 0,
        EMLOG_WARN = 1,
        EMLOG_FAULT = 2
};
```

The level is stored as a `uint16_t`, without a range check.

On GCC and Clang, `EMLOG_PRINTF_LIKE(fmt_idx, arg_idx)` applies the `format` attribute to `emlog_event`, so `-Wformat` checks every call; on other compilers it expands to nothing. Use it on your own wrappers in the same way.

<!-- snippet: body -->
```c
EMLOG_ONCE(&log_ctx, EMLOG_WARN, "calibration missing, using defaults");
```

`EMLOG_ONCE(ctx, level, ...)` calls `emlog_event` the first time execution reaches it and never again until reset. Its guard is a `static` flag at the expansion site, so each call site is tracked separately, but all contexts and callers that reach the same site share it. The flag is set even when the call records nothing because an argument is NULL. It is not interlocked, so two contexts that reach a site for the first time together can both record.

## Interrupt path

```c
bool emlog_event_isr(struct emlog_ctx *ctx, enum emlog_level level,
                     const char *fmt, uint32_t arg0, uint32_t arg1);
uint16_t emlog_drain(struct emlog_ctx *ctx);
uint_least8_t emlog_get_isr_dropped(const struct emlog_ctx *ctx);
```

`emlog_event_isr` takes a timestamp and copies it, `level`, the `fmt` pointer and both arguments into the pending ring, without formatting, locking or waiting. It returns `true` when the entry is queued. It returns `false` when `ctx`, `fmt` or the timestamp function is NULL, or when the pending ring already holds `EMLOG_ISR_ENTRIES` entries; only the last case counts as a dropped entry.

`fmt` is read when the entry is drained, not when it is posted, so it must have static storage duration; a string literal is the usual choice. Its conversions must consume `uint32_t` values, through `PRIu32`, `PRIx32` and the other `<inttypes.h>` macros, and it may use fewer than two.

`emlog_drain` formats the pending entries into the ring, oldest first, as `emlog_event` would. Each entry keeps the timestamp taken when it was posted, so a drained entry can be older than the entry before it in the ring. It returns how many it moved, at most `EMLOG_ISR_ENTRIES`, and returns 0 for a NULL context. Entries posted while it runs wait for the next call.

`emlog_get_isr_dropped` returns how many entries `emlog_event_isr` has rejected because the pending ring was full, since `emlog_init`. It stops at `UINT_LEAST8_MAX`, which is 255 on most targets and larger where `CHAR_BIT` is larger. It returns 0 for a NULL context.

## Inspection

```c
uint16_t emlog_get_count(const struct emlog_ctx *ctx);
const struct emlog_entry *emlog_get_entry(const struct emlog_ctx *ctx,
                                          uint16_t idx);
const struct emlog_entry *emlog_get_buffer(const struct emlog_ctx *ctx,
                                           uint16_t *count);
```

`emlog_get_count` returns the number of stored entries, from 0 up to `EMLOG_ENTRIES`, and 0 for a NULL context.

`emlog_get_entry` returns the `idx`-th oldest entry, so index 0 is the oldest and `emlog_get_count() - 1` the newest. It returns NULL when `ctx` is NULL or `idx` is not below the count. The pointer refers to the ring itself, so a later `emlog_event` or `emlog_drain` can overwrite the entry.

`emlog_get_buffer` returns the ring's entry array, `EMLOG_ENTRIES` elements long, and writes the number of stored entries to `*count` when `count` is non-NULL. The array is in storage order: once the ring has wrapped, the oldest entry is no longer at index 0. Use `emlog_get_entry` for chronological order. For a NULL context it returns NULL and writes 0.

<!-- snippet: skip -->
```c
struct emlog_entry {
        uint32_t timestamp;
        uint16_t level;
        char msg[EMLOG_MSG_LEN];
};
```

## Configuration

Each option is a macro in `emlog_conf.h` with a default that a `-D` definition replaces. Every one of them changes the layout of `struct emlog_ctx`, so the library and all its callers need the same values; [integration.md](integration.md#configuration) explains how.

`EMLOG_ENTRIES` is the number of formatted entries the ring keeps, 64 by default, from 1 to 65535.

`EMLOG_MSG_LEN` is the size of each message buffer, including the NUL terminator, 64 by default, and at least 1.

`EMLOG_ISR_ENTRIES` is the number of entries `emlog_event_isr` can hold until the next `emlog_drain`, 8 by default, from 1 to 254. The pending ring has one slot more than this.

A value outside its range fails to compile. The context needs about `EMLOG_ENTRIES` × `sizeof(struct emlog_entry)` plus `(EMLOG_ISR_ENTRIES + 1)` × `sizeof(struct emlog_pending)` bytes: 4848 bytes with the defaults on x86-64.

`EMLOG_USE_GNU_ATOMICS`, `EMLOG_USE_C11_ATOMICS` and `EMLOG_USE_NO_ATOMICS` force the backend of the interrupt path. Define exactly one, to any value; defining two fails to compile. Without one, GCC and Clang use `__atomic`, other C11 compilers with atomics use `<stdatomic.h>`, and the rest use `volatile`. [design.md](design.md#backends) describes each.

`EMLOG_FENCE_ACQUIRE()` and `EMLOG_FENCE_RELEASE()` take effect only under the `volatile` backend, where they default to nothing. Define them as the target's memory barriers when the interrupt and the draining context can run on different cores. They take effect when `src/emlog.c` is compiled.

## Recipes

### Fault from an interrupt

The handler records the raw values and returns; the background loop formats them when it has time.

```c
#include <inttypes.h>

static volatile uint32_t adc_sample;
static volatile uint32_t adc_channel;

void
adc_isr(void)
{
        if (adc_sample > 4000u) {
                (void)emlog_event_isr(&log_ctx, EMLOG_FAULT,
                                      "adc %" PRIu32 " on ch %" PRIu32,
                                      adc_sample, adc_channel);
        }
}

void
background_loop(void)
{
        (void)emlog_drain(&log_ctx);
}
```

Drain often enough that the pending ring cannot fill between calls: at least once per `EMLOG_ISR_ENTRIES` posts. A non-zero `emlog_get_isr_dropped` means it did fill.

### Dumping the log

When the firmware has an output, write the ring out oldest first:

```c
#include <stdio.h>

static const char *
level_name(uint16_t level)
{
        static const char *const names[] = {"INFO", "WARN", "FAULT"};

        return (level <= (uint16_t)EMLOG_FAULT) ? names[level] : "?";
}

void
log_dump(void)
{
        (void)emlog_drain(&log_ctx);
        for (uint16_t i = 0u; i < emlog_get_count(&log_ctx); ++i) {
                const struct emlog_entry *e = emlog_get_entry(&log_ctx, i);

                printf("%10lu %-5s %s\n", (unsigned long)e->timestamp,
                       level_name(e->level), e->msg);
        }
}
```

Without an output, read the context with a debugger. Its `buffer` member holds the entries, `count` how many are valid, and `head` the slot the next entry will use, so the newest entry is the one before `head`.

## Version

`emlog.h` defines the release it belongs to, so a copied-in header still identifies itself:

<!-- snippet: skip -->
```c
#define EMLOG_VERSION_MAJOR 1
#define EMLOG_VERSION_MINOR 0
#define EMLOG_VERSION_PATCH 0
#define EMLOG_VERSION "1.0.0"
```

`EMLOG_VERSION_NUMBER` is `MAJOR * 10000 + MINOR * 100 + PATCH` as a `long`, so it holds any version where `int` is 16 bits. Compare it in the preprocessor to require a minimum release:

```c
#if EMLOG_VERSION_NUMBER < 10000L
#error "emlog 1.0.0 or later is required"
#endif
```

`emlog_version.h` is a deprecated header that includes `emlog.h`, kept so existing includes compile until the next major release.
