/**
 * SPDX-License-Identifier: MIT
 *
 * @file: emlog_conf.h
 *
 * @brief
 *    Configuration macros for the embedded logging facility.
 *
 *    Override any macro before including @c emlog.h (e.g. via a compiler
 *    @c -D flag or a project-level config header) to change the defaults.
 */

#ifndef EMLOG_CONF_H
#define EMLOG_CONF_H

#ifndef EMLOG_MSG_LEN
/**
 * @brief Maximum formatted message length, including the NUL terminator.
 *
 * Default: 64 characters.
 */
#define EMLOG_MSG_LEN (64u)
#endif

#ifndef EMLOG_ENTRIES
/**
 * @brief Number of entries retained in the ring buffer.
 *
 * Default: 64 entries.
 */
#define EMLOG_ENTRIES (64u)
#endif

#ifndef EMLOG_ISR_ENTRIES
/**
 * @brief Number of entries emlog_event_isr() can hold until emlog_drain().
 *
 * Range [1, 254]. Default: 8 entries.
 */
#define EMLOG_ISR_ENTRIES (8u)
#endif

/*
 * Atomic backend for the emlog_event_isr() ring indices. Define exactly one
 * of EMLOG_USE_GNU_ATOMICS, EMLOG_USE_C11_ATOMICS or EMLOG_USE_NO_ATOMICS
 * to override automatic selection, which tries in order:
 *   GCC/Clang __atomic builtins -> C11 <stdatomic.h> -> volatile.
 *
 * EMLOG_USE_NO_ATOMICS stores the indices in volatile objects, and the
 * pending entries are volatile too, so the compiler keeps every access in
 * program order. That is sufficient on a single core where an interrupt
 * pre-empts the draining context. On a multi-core target without atomics,
 * define EMLOG_FENCE_ACQUIRE and EMLOG_FENCE_RELEASE as the hardware
 * barriers that order memory between cores.
 *
 * The library and every translation unit that includes emlog.h must select
 * the same backend.
 */
#if !defined(EMLOG_USE_GNU_ATOMICS) && !defined(EMLOG_USE_C11_ATOMICS)         \
    && !defined(EMLOG_USE_NO_ATOMICS)
#if defined(__GNUC__) || defined(__clang__)
#define EMLOG_USE_GNU_ATOMICS 1
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)               \
    && !defined(__STDC_NO_ATOMICS__)
#define EMLOG_USE_C11_ATOMICS 1
#else
#define EMLOG_USE_NO_ATOMICS 1
#endif
#endif

#if (defined(EMLOG_USE_GNU_ATOMICS) + defined(EMLOG_USE_C11_ATOMICS)           \
     + defined(EMLOG_USE_NO_ATOMICS))                                          \
    != 1
#error "emlog: define exactly one atomic backend"
#endif

#ifndef EMLOG_FENCE_ACQUIRE
#define EMLOG_FENCE_ACQUIRE()                                                  \
        do {                                                                   \
        } while (0)
#endif

#ifndef EMLOG_FENCE_RELEASE
#define EMLOG_FENCE_RELEASE()                                                  \
        do {                                                                   \
        } while (0)
#endif

#endif /* EMLOG_CONF_H */
