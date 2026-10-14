/**
 * SPDX-License-Identifier: MIT
 *
 * @file: emlog.h
 *
 * @brief
 *    Lightweight, MISRA C:2023 aware RAM log buffer for embedded systems.
 */

#ifndef EMLOG_H
#define EMLOG_H

#include <stdbool.h>
#include <stdint.h>

#include "emlog_conf.h"

/* The ring-buffer state (head, count) is held in uint16_t; reject
 * configurations the implementation cannot represent. */
#if (EMLOG_ENTRIES < 1u) || (EMLOG_ENTRIES > 65535u)
#error "EMLOG_ENTRIES must be in the range [1, 65535]"
#endif

#if (EMLOG_MSG_LEN < 1u)
#error "EMLOG_MSG_LEN must be at least 1 (the NUL terminator)"
#endif

/* The pending-ring indices are uint_least8_t and the ring holds one spare
 * slot. */
#if (EMLOG_ISR_ENTRIES < 1u) || (EMLOG_ISR_ENTRIES > 254u)
#error "EMLOG_ISR_ENTRIES must be in the range [1, 254]"
#endif

#if defined(EMLOG_USE_C11_ATOMICS)
#define EMLOG__ATOMIC _Atomic
#elif defined(EMLOG_USE_GNU_ATOMICS)
#define EMLOG__ATOMIC
#else
#define EMLOG__ATOMIC volatile
#endif

/**
 * @defgroup emlog_api Embedded Logging Facility
 *
 * @brief
 *   Lightweight, MISRA C:2023 aware RAM log buffer for embedded systems.
 *
 *   This module provides a minimal, robust API for event and fault logging
 *   on microcontrollers and safety-critical firmware. Logs are stored in a
 *   circular buffer in RAM and can be inspected with a debugger or dumped
 *   at runtime if desired.
 *
 *   Features:
 *   - User-supplied context for flexible instancing
 *   - Levels: EMLOG_INFO, EMLOG_WARN, EMLOG_FAULT
 *   - One-shot logging macro (EMLOG_ONCE)
 *   - Defensive: NULL pointer safe
 *   - No dynamic memory, no heap, no OS dependency
 *   - Every identifier carries the emlog_ or EMLOG_ prefix, so the header
 *     can be included alongside <syslog.h>
 *   - Interrupt-safe deferred path: emlog_event_isr() records the format
 *     and two arguments lock-free, and emlog_drain() formats them later
 *
 *   **Example Usage:**
 *   @code
 *   #include <stdio.h>
 *   #include "emlog.h"
 *
 *   // User supplies a context (buffer, counters)
 *   static struct emlog_ctx my_log;
 *
 *   uint32_t my_get_ticks(void) {
 *       // Return timestamp in your preferred units
 *       return timer_ticks;
 *   }
 *
 *   void app_init(void) {
 *       emlog_init(&my_log, my_get_ticks);
 *   }
 *
 *   void some_function(void) {
 *       emlog_event(&my_log, EMLOG_INFO, "System started, mode=%u", mode);
 *       EMLOG_ONCE(&my_log, EMLOG_WARN, "First call to some_function()");
 *   }
 *
 *   void print_log(void) {
 *       for (uint16_t i = 0u; i < emlog_get_count(&my_log); ++i) {
 *           const struct emlog_entry *e = emlog_get_entry(&my_log, i);
 *           printf("[%lu] : %s : %s\n",
 *               (unsigned long)e->timestamp,
 *               (e->level == (uint16_t)EMLOG_INFO)   ? "INFO"
 *               : (e->level == (uint16_t)EMLOG_WARN) ? "WARN"
 *                                                    : "FAULT",
 *               e->msg);
 *       }
 *   }
 *   @endcode
 *
 * @{
 */

/**
 * @def EMLOG_PRINTF_LIKE
 * @brief Enable compile-time format-string checking where supported.
 *
 * Expands to the GNU @c format attribute on GCC/clang and to nothing on
 * other compilers, so the API remains strict ISO C11.
 */
#if defined(__GNUC__)
#define EMLOG_PRINTF_LIKE(fmt_idx, arg_idx)                                    \
        __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define EMLOG_PRINTF_LIKE(fmt_idx, arg_idx)
#endif

/**
 * @brief Log level enum.
 */
enum emlog_level {
        EMLOG_INFO = 0,
        EMLOG_WARN = 1,
        EMLOG_FAULT = 2
};

/**
 * @brief Log entry structure.
 */
struct emlog_entry {
        uint32_t timestamp;
        uint16_t level;
        char msg[EMLOG_MSG_LEN];
};

/**
 * @brief Entry recorded by emlog_event_isr() and formatted by emlog_drain().
 */
struct emlog_pending {
        const char *fmt;
        uint32_t timestamp;
        uint32_t arg[2];
        uint16_t level;
};

/**
 * @brief Log context, holding buffer and state.
 *
 * @details
 * - ::buffer, ::head, ::count  the formatted ring, owned by the context
 *   that calls emlog_event() and emlog_drain().
 * - ::isr_ring, ::isr_head     pending entries and the next free slot,
 *   written by emlog_event_isr().
 * - ::isr_tail                 the oldest pending slot, written by
 *   emlog_drain().
 * - ::isr_dropped              entries emlog_event_isr() rejected because
 *   the pending ring was full, saturating at UINT_LEAST8_MAX.
 */
struct emlog_ctx {
        struct emlog_entry buffer[EMLOG_ENTRIES];
        uint16_t head;
        uint16_t count;
        uint32_t (*timestamp_fn)(void);
        volatile struct emlog_pending isr_ring[EMLOG_ISR_ENTRIES + 1u];
        EMLOG__ATOMIC uint_least8_t isr_head;
        EMLOG__ATOMIC uint_least8_t isr_tail;
        EMLOG__ATOMIC uint_least8_t isr_dropped;
};

/**
 * @brief Initialise the log context.
 *
 * @param ctx           Pointer to user-supplied log context.
 * @param timestamp_fn  Pointer to user-supplied timestamp function.
 */
void emlog_init(struct emlog_ctx *ctx, uint32_t (*timestamp_fn)(void));

/**
 * @brief Add a log entry.
 *
 * @param ctx       Pointer to log context.
 * @param level     Log level (EMLOG_INFO, EMLOG_WARN, EMLOG_FAULT).
 * @param fmt       printf-style format string.
 * @param ...       Arguments for format string.
 *
 * @note            The formatted message is truncated to fit
 *                  @c EMLOG_MSG_LEN - 1 characters and is always
 *                  NUL-terminated.
 */
void emlog_event(struct emlog_ctx *ctx, enum emlog_level level, const char *fmt,
                 ...) EMLOG_PRINTF_LIKE(3, 4);

/**
 * @brief Get the number of valid log entries in the buffer.
 *
 * @param ctx       Pointer to log context.
 *
 * @return          Number of valid log entries.
 */
uint16_t emlog_get_count(const struct emlog_ctx *ctx);

/**
 * @brief Get the idx-th oldest log entry.
 *
 * @param ctx       Pointer to log context.
 * @param idx       Index (0 = oldest).
 *
 * @return          Pointer to log entry, or NULL if out of bounds.
 */
const struct emlog_entry *emlog_get_entry(const struct emlog_ctx *ctx,
                                          uint16_t idx);

/**
 * @brief Return pointer to log buffer for direct inspection.
 *
 * @param ctx       Pointer to log context.
 * @param count     If non-NULL, writes number of valid entries.
 *
 * @return          Pointer to emlog_entry array.
 *
 * @note            Entries are returned in physical (storage) order. Once
 *                  the ring buffer has wrapped, this differs from
 *                  chronological order; use emlog_get_entry() for
 *                  oldest-first access.
 */
const struct emlog_entry *emlog_get_buffer(const struct emlog_ctx *ctx,
                                           uint16_t *count);

/**
 * @brief Record an entry from interrupt context, deferring the formatting.
 *
 * Captures the timestamp, level, format pointer and both arguments into
 * the pending ring without locking, formatting or blocking. emlog_drain()
 * formats them into the log later.
 *
 * @param ctx       Pointer to log context.
 * @param level     Log level.
 * @param fmt       printf-style format string with static storage
 *                  duration, such as a string literal; it is read when the
 *                  entry is drained. Its conversions consume up to two
 *                  uint32_t arguments (PRIu32, PRIx32); unused arguments
 *                  are ignored.
 * @param arg0      First format argument.
 * @param arg1      Second format argument.
 *
 * @return          true if the entry was queued; false if an argument is
 *                  NULL, the context has no timestamp function, or the
 *                  pending ring is full (the entry is then counted by
 *                  emlog_get_isr_dropped()).
 *
 * @note            Single producer: one execution context at a time may
 *                  call this function, concurrently with the context that
 *                  calls emlog_drain(). Interrupts of different priorities
 *                  that both log must not pre-empt each other inside the
 *                  call. The timestamp function must be safe to call from
 *                  that context.
 */
bool emlog_event_isr(struct emlog_ctx *ctx, enum emlog_level level,
                     const char *fmt, uint32_t arg0, uint32_t arg1);

/**
 * @brief Format pending emlog_event_isr() entries into the log.
 *
 * Entries keep the timestamp captured when they were posted and are
 * appended in posting order after any entry already in the log.
 *
 * @param ctx       Pointer to log context.
 *
 * @return          Number of entries moved into the log.
 *
 * @note            Call from the context that owns emlog_event(), never
 *                  concurrently with it.
 */
uint16_t emlog_drain(struct emlog_ctx *ctx);

/**
 * @brief Number of emlog_event_isr() entries dropped because the pending
 *        ring was full.
 *
 * @param ctx       Pointer to log context.
 *
 * @return          Entries dropped since emlog_init(), saturating at
 *                  UINT_LEAST8_MAX; 0 for a NULL context.
 */
uint_least8_t emlog_get_isr_dropped(const struct emlog_ctx *ctx);

/**
 * @def EMLOG_ONCE
 * @brief Log a message at most once per code location per reset.
 *
 * This macro ensures that a log statement is only recorded once for the
 * lifetime of the program. The guard flag lives in function-local static
 * storage at the expansion site, so each code location is tracked
 * independently and the flag is cleared only by a reset.
 *
 * Example usage:
 * @code
 * void state_run(struct emlog_ctx *ctx) {
 *     EMLOG_ONCE(ctx, EMLOG_WARN, "Waiting for module ready...");
 * }
 * @endcode
 *
 * @param ctx       Pointer to log context.
 * @param level     Log level.
 * @param ...       printf-style format string and its arguments.
 *
 * @note            The guard flag is not interlocked; concurrent first
 *                  calls from multiple execution contexts may log more
 *                  than once.
 */
#define EMLOG_ONCE(ctx, level, ...)                                            \
        do {                                                                   \
                static uint_least8_t emlog_once_done = 0u;                     \
                if (emlog_once_done == 0u) {                                   \
                        emlog_event((ctx), (level), __VA_ARGS__);              \
                        emlog_once_done = 1u;                                  \
                }                                                              \
        } while (0)

/**
 * Close group: emlog_api
 * @}
 */

#endif /* EMLOG_H */
