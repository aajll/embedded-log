# Design

`embedded-log` keeps a bounded, human-readable record of what firmware did, in RAM that a debugger can read while the target runs or after it stops. This document describes how the ring and the interrupt path work, what each backend guarantees, and how the library is verified. The API is in [api.md](api.md).

## Goals

- Fixed memory: everything lives in a caller-owned `struct emlog_ctx` sized at compile time, with no heap.
- Readable in place: entries are formatted text with a timestamp and a level, so a memory view of the context is the log.
- Safe to record from an interrupt without disabling interrupts.
- Portable C11, including targets where `CHAR_BIT` is 16 and targets without atomics.

Persistence across power cycles, transport, filtering and log rotation are out of scope. A project that needs them builds them around the context.

## Formatted ring

The ring is an array of `EMLOG_ENTRIES` entries with two `uint16_t` fields: `head`, the slot the next entry uses, and `count`, the number of valid entries. Recording claims the slot at `head`, advances `head` modulo `EMLOG_ENTRIES`, and increments `count` until it reaches `EMLOG_ENTRIES`. A full ring therefore overwrites its oldest entry, which keeps the most recent history, the part that explains a fault.

The `idx`-th oldest entry is at `(head + EMLOG_ENTRIES - count + idx) % EMLOG_ENTRIES`, computed in `uint32_t` so the sum cannot overflow. `emlog_get_buffer` exposes the array in storage order for a debugger or a bulk copy.

Each entry stores the message already formatted. Formatting at record time costs CPU on the recording path, but the stored entry depends on nothing else: its arguments may go out of scope, and no format string has to stay in memory for a debugger to read it. `emlog_event` formats with `vsnprintf`, bounded by `EMLOG_MSG_LEN`, so a long message is truncated and still terminated. A negative return, an encoding error, leaves an empty message rather than whatever the formatter wrote. The formatter belongs to the C library, so its stack depth, its floating-point support and its reentrancy are the C library's.

The ring is not synchronised. `emlog_event` reads and writes `head` and `count` without atomics, so it must never run concurrently with itself, `emlog_drain` or a reader. That is also why it is not callable from an interrupt that can pre-empt another caller.

## Interrupt path

`emlog_event_isr` solves the interrupt case by not touching the formatted ring at all. It writes into a second ring of `struct emlog_pending` slots, each holding the format pointer, a timestamp, two `uint32_t` arguments and the level, and `emlog_drain` later formats those slots into the main ring from the owning context. The interrupt does no formatting, so its cost is a timestamp call and a few stores, and the main ring keeps its single owner.

The pending ring is a single-producer, single-consumer queue with `EMLOG_ISR_ENTRIES + 1` slots. The producer writes only `isr_head` and the consumer only `isr_tail`, so neither needs a read-modify-write. One slot always stays empty, which distinguishes the two states without a shared count: `isr_head == isr_tail` is empty, and the slot after `isr_head` being `isr_tail` is full. The indices are `uint_least8_t`, which bounds `EMLOG_ISR_ENTRIES` at 254.

The producer:

1. loads `isr_head` (relaxed, since only it writes it) and `isr_tail` with acquire ordering;
2. if the ring is full, counts the drop and returns `false`;
3. writes the slot at `isr_head`;
4. stores the next index to `isr_head` with release ordering.

The consumer, for each slot from `isr_tail` up to the `isr_head` it loaded with acquire ordering at the start of the call:

1. reads the slot and formats it into the main ring;
2. stores the next index to `isr_tail` with release ordering.

Two orderings make this correct. The release store of `isr_head` and the consumer's acquire load of it put the slot's contents before the consumer's reads, so the consumer never formats a half-written slot. The release store of `isr_tail` and the producer's acquire load of it put the consumer's reads of a slot before the producer's next write to it, so the producer never overwrites a slot that is still being formatted. A consumer that took a fresh `isr_head` for every slot would also be correct; taking it once bounds each call to the entries present when it started.

The dropped-entry counter is written only by the producer, with a relaxed load and store, and saturates at `UINT_LEAST8_MAX`. The owner reads it relaxed, so its value can lag the producer by one post.

## Backends

Every access to an index goes through a macro in `src/emlog.c`, and `emlog_conf.h` chooses which definitions those macros get. Without an explicit choice, GCC and Clang get `__atomic`, other C11 compilers that provide atomics get `<stdatomic.h>`, and the rest get `volatile`.

### `__atomic` backend

The indices are plain `uint_least8_t` accessed with `__atomic_load_n` and `__atomic_store_n` at the orderings above. A static assertion requires `__atomic_always_lock_free` for that size, because an atomic implemented with a lock could deadlock when an interrupt pre-empts the lock holder.

### C11 backend

The indices are `_Atomic uint_least8_t`, accessed with `atomic_load_explicit` and `atomic_store_explicit`. A static assertion requires `ATOMIC_CHAR_LOCK_FREE == 2`, always lock-free, for the same reason. An `_Atomic` type may differ in size or alignment from its plain form, which is one reason the library and its callers must select the same backend.

### `volatile` backend

The indices and the slots are `volatile`, and the macros are plain reads and writes. `volatile` forces the compiler to emit every access to them in program order, and a single core always observes its own accesses in program order. When the producer is an interrupt on the same core as the consumer, that is enough: the interrupt sees either all of the consumer's writes up to a point or none of them, and vice versa.

It is not enough when the two contexts run on different cores, because the hardware may then make the writes visible in another order. `EMLOG_FENCE_ACQUIRE()` runs after each acquire load of an index and `EMLOG_FENCE_RELEASE()` before each release store; both do nothing by default, and a multi-core target without C11 atomics defines them as its barrier instructions. The backend also relies on each index being read and written in one access, which holds for `uint_least8_t` on any target, since it is a single `char`.

## Context rules

- The owner, the context that calls `emlog_event`, `emlog_drain`, `EMLOG_ONCE` and the inspection functions, never runs those calls concurrently with each other.
- One producer context at a time calls `emlog_event_isr`, and it may pre-empt the owner at any point, or run on another core with the atomic backends. Two interrupts of different priorities that both post would both claim the same slot if one pre-empted the other inside the call, so they must not nest there.
- The timestamp function is called from both contexts, so it must be safe to call from the producer.
- `emlog_init` runs before any other call, and never while an interrupt may post.
- Every format string passed to `emlog_event_isr` outlives the entry, because it is dereferenced when the entry is drained.

## Failure behaviour

| Condition | Result |
| --- | --- |
| NULL context, format or timestamp function | The call does nothing; `emlog_event_isr` returns `false` without counting a drop. |
| Message longer than `EMLOG_MSG_LEN - 1` | Truncated and terminated. |
| Formatter encoding error | Empty message. |
| Main ring full | The oldest entry is replaced. |
| Pending ring full | `emlog_event_isr` returns `false` and the drop is counted, saturating. |
| Level outside `enum emlog_level` | Stored as given. |
| Configuration out of range | Compile error. |
| Library and caller configured differently | Undetected; memory corruption. |

Timestamps are stored as the function returns them. Their unit and their wrap at 2^32 are the caller's.

## MISRA C:2023

The library is analysed with [`misch`](https://pypi.org/project/misch/), a cppcheck-based MISRA C:2023 checker, over `include/` and `src/`. Each backend compiles different code, so CI analyses each one, and each has a baseline of accepted findings in `analysis/baseline/`. CI fails on any finding outside the matching baseline. The version macros in `emlog.h` carry an inline deviation of Rule 2.5 (unused macro), because they exist for consumers and the library does not use them.

| Backend | Findings | Rules |
| --- | --- | --- |
| `__atomic` | 39 | 2.5, 8.7, 15.5, 17.1, 17.3, 21.6 |
| C11 | 26 | 2.5, 8.7, 15.5, 17.1, 21.6 |
| `volatile` | 32 | 2.5, 8.7, 13.4, 15.5, 17.1, 21.6 |

- Rules 17.1 (`<stdarg.h>`) and 21.6 (`<stdio.h>`) follow from the `printf`-style API and from deferred formatting. Both formatter calls are bounded by the entry size.
- Rule 17.3 under `__atomic` reports the compiler builtins, which have no prototype for the analyser to see.
- Rule 13.4 under `volatile` reports the index store macros, which are assignments used as expressions.
- Rule 15.5 reports the early returns that reject invalid arguments.
- Rule 8.7 reports the public functions, because the analysis sees no caller outside `src/emlog.c`, and Rule 2.5 reports `EMLOG_ONCE` and the backend selection macros, which the library defines for its callers.

The baselines record these findings as they stand. Formal compliance needs a certified analyser and a deviation record from the consuming project.

## Verification

The tests run on the host:

| Test | What it checks |
| --- | --- |
| `init`, `event`, `read` | Initialisation, NULL handling, truncation, encoding errors, wrap and overwrite, chronological and storage order, `EMLOG_ONCE`. |
| `legacy`, `legacy_conf` | The deprecated names and configuration forward to the `emlog` API; `legacy_conf` is a set of compile-time assertions. |
| `syslog_coexistence` | `emlog.h` and `<syslog.h>` compile together. Built only where `<syslog.h>` exists. |
| `isr_gnu`, `isr_c11`, `isr_no` | The interrupt path under each backend, each against its own build of the library: deferred formatting, posting order, full ring, drop counting and saturation, encoding errors, invalid arguments. |
| `isr_gnu`, `isr_c11` concurrent case | One thread posts 200000 entries while the main thread drains. Every accepted entry must arrive exactly once, in order, with both arguments intact. |
| `embedded-log docs examples` | Every C block in the README, `CONTRIBUTING.md` and `docs/` compiles against the public headers. |

CI runs every test under AddressSanitizer and UndefinedBehaviorSanitizer and again under ThreadSanitizer, gates line and branch coverage of `src/` and `include/` at 100 percent, builds the library in release mode, checks formatting, and runs the MISRA analysis for each backend. It also compiles the library with warnings as errors under TI C2000 CGT 22.6.2 and 25.11.1, in both the COFF and EABI formats; that compiler selects the `volatile` backend.

On an AArch64 host, which reorders memory accesses that x86-64 keeps in order, the full suite passes in a release build and under ThreadSanitizer. With the acquire and release orderings of the `__atomic` backend weakened to relaxed, ThreadSanitizer reports a data race on the pending slots and `isr_gnu` fails; the same weakened build passed 30 release-mode runs on that host. The concurrent case therefore detects lost or duplicated entries, and ThreadSanitizer, not the release-mode run, detects a missing ordering.

The `volatile` backend's concurrency is not tested on the host, where threads run in parallel and the backend is not meant to hold. Its single-context behaviour is covered by `isr_no`, and its ordering argument rests on the single-core reasoning above.
