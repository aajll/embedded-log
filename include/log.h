/**
 * SPDX-License-Identifier: MIT
 *
 * @file: log.h
 *
 * @brief
 *    Lightweight, MISRA C-compliant RAM log buffer for embedded systems.
 */

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdint.h>

/**
 * @defgroup log_api Embedded Logging Facility
 *
 * @brief
 *   Lightweight, MISRA C-compliant RAM log buffer for embedded systems.
 *
 *   This module provides a minimal, robust API for event and fault logging
 *   on microcontrollers and safety-critical firmware. Logs are stored in a
 *   circular buffer in RAM and can be inspected with a debugger or dumped
 *   at runtime if desired.
 *
 *   Features:
 *   - User-supplied context for flexible instancing
 *   - Levels: INFO, WARN, FAULT
 *   - One-shot logging macro (LOG_ONCE)
 *   - Defensive: NULL pointer safe
 *   - No dynamic memory, no heap, no OS dependency
 *   - Designed for integration as a Meson subproject
 *
 *   **Example Usage:**
 *   @code
 *   #include "log.h"
 *
 *   // User supplies a context (buffer, counters)
 *   static struct log_ctx my_log;
 *
 *   uint32_t my_get_ticks(void) {
 *       // Return timestamp in your preferred units
 *       return timer_ticks;
 *   }
 *
 *   void app_init(void) {
 *       log_init(&my_log, my_get_ticks);
 *   }
 *
 *   void some_function(void) {
 *       log_event(&my_log, INFO, "System started, mode=%u", mode);
 *       LOG_ONCE(&my_log, WARN, "First call to some_function()");
 *   }
 *
 *   void print_log(void) {
 *       for (uint16_t i = 0; i < log_get_count(&my_log); ++i) {
 *           const struct log_entry *e = log_get_entry(&my_log, i);
 *           printf("[%lu] : %s : %s\n",
 *               (unsigned long)e->timestamp,
 *               (e->level == INFO) ? "INFO" : (e->level == WARN) ? "WARN" :
 *                                    "FAULT", e->msg);
 *       }
 *   }
 *   @endcode
 *
 * @{
 */

#define LOG_MSG_LEN (48u)
#define LOG_ENTRIES (50u)

/**
 * @brief Log level enum.
 */
enum log_level {
        INFO = 0u,
        WARN = 1u,
        FAULT = 2u
};

/**
 * @brief Log entry structure.
 */
struct log_entry {
        uint32_t timestamp;
        uint16_t level;
        char msg[LOG_MSG_LEN];
};

/**
 * @brief Log context, holding buffer and state.
 */
struct log_ctx {
        struct log_entry buffer[LOG_ENTRIES];
        uint16_t head;
        uint16_t count;
        uint32_t (*timestamp_fn)(void);
};

/**
 * @brief Initialize the log context.
 *
 * @param ctx           Pointer to user-supplied log context.
 * @param timestamp_fn  Pointer to user-supplied timestamp function.
 */
void log_init(struct log_ctx *ctx, uint32_t (*timestamp_fn)(void));

/**
 * @brief Add a log entry.
 *
 * @param ctx       Pointer to log context.
 * @param level     Log level (INFO, WARN, FAULT).
 * @param fmt       printf-style format string.
 * @param ...       Arguments for format string.
 *
 * @note            Automatically appends a NULL terminating character.
 */
void log_event(struct log_ctx *ctx, enum log_level level, const char *fmt, ...);

/**
 * @brief Get the number of valid log entries in the buffer.
 *
 * @param ctx       Pointer to log context.
 *
 * @return          Number of valid log entries.
 */
uint16_t log_get_count(const struct log_ctx *ctx);

/**
 * @brief Get the idx-th oldest log entry.
 *
 * @param ctx       Pointer to log context.
 * @param idx       Index (0 = oldest).
 *
 * @return          Pointer to log entry, or NULL if out of bounds.
 */
const struct log_entry *log_get_entry(const struct log_ctx *ctx, uint16_t idx);

/**
 * @brief Return pointer to log buffer for direct inspection.
 *
 * @param ctx       Pointer to log context.
 * @param count     If non-NULL, writes number of valid entries.
 *
 * @return          Pointer to log_entry array.
 */
const struct log_entry *log_get_buffer(const struct log_ctx *ctx,
                                       uint16_t *count);

/**
 * @def LOG_ONCE
 * @brief Log a message at most once per code location per reset.
 *
 * This macro ensures that a log statement is only recorded once for the
 * lifetime of the program or until the enclosing function's static variable
 * context is reset.
 *
 * Example usage:
 * @code
 * void state_run(struct log_ctx *ctx) {
 *     LOG_ONCE(ctx, WARN, "Waiting for module ready...");
 * }
 * @endcode
 *
 * @param ctx       Pointer to log context.
 * @param level     Log level.
 * @param fmt       printf-style format string.
 * @param ...       Arguments for format string.
 */
#define LOG_ONCE(ctx, level, fmt, ...)                                         \
        do {                                                                   \
                static uint8_t _logged = 0u;                                   \
                if (_logged == 0u) {                                           \
                        log_event(ctx, level, fmt, ##__VA_ARGS__);             \
                        _logged = 1u;                                          \
                }                                                              \
        } while (0)

/**
 * Close group: log_api
 * @}
 */

#endif /* LOG_H */
