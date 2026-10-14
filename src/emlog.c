/**
 * SPDX-License-Identifier: MIT
 *
 * @file: emlog.c
 *
 * @brief
 *    Implementation of the embedded logging facility.
 *
 *    MISRA C:2023 deviations, accepted by design:
 *    - Rule 17.1 (stdarg.h) and Rule 21.6 (stdio.h): the printf-style
 *      emlog_event API requires va_list and vsnprintf, and emlog_drain
 *      formats deferred entries with snprintf. Both are bounded and the
 *      result is always NUL-terminated.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "emlog.h"

#if defined(EMLOG_USE_C11_ATOMICS)
#include <stdatomic.h>
#endif

/*
 * Index access for the pending ring. The producer publishes a slot with a
 * release store of isr_head and the drain frees it with a release store of
 * isr_tail; each side observes the other's index with an acquire load.
 */
#if defined(EMLOG_USE_GNU_ATOMICS)
#define EMLOG__LOAD_RELAXED(ptr) __atomic_load_n((ptr), __ATOMIC_RELAXED)
#define EMLOG__LOAD_ACQUIRE(ptr) __atomic_load_n((ptr), __ATOMIC_ACQUIRE)
#define EMLOG__STORE_RELAXED(ptr, val)                                         \
        __atomic_store_n((ptr), (val), __ATOMIC_RELAXED)
#define EMLOG__STORE_RELEASE(ptr, val)                                         \
        __atomic_store_n((ptr), (val), __ATOMIC_RELEASE)
#define EMLOG__FENCE_AFTER_ACQUIRE()                                           \
        do {                                                                   \
        } while (0)
#define EMLOG__FENCE_BEFORE_RELEASE()                                          \
        do {                                                                   \
        } while (0)
#elif defined(EMLOG_USE_C11_ATOMICS)
#define EMLOG__LOAD_RELAXED(ptr)                                               \
        atomic_load_explicit((ptr), memory_order_relaxed)
#define EMLOG__LOAD_ACQUIRE(ptr)                                               \
        atomic_load_explicit((ptr), memory_order_acquire)
#define EMLOG__STORE_RELAXED(ptr, val)                                         \
        atomic_store_explicit((ptr), (val), memory_order_relaxed)
#define EMLOG__STORE_RELEASE(ptr, val)                                         \
        atomic_store_explicit((ptr), (val), memory_order_release)
#define EMLOG__FENCE_AFTER_ACQUIRE()                                           \
        do {                                                                   \
        } while (0)
#define EMLOG__FENCE_BEFORE_RELEASE()                                          \
        do {                                                                   \
        } while (0)
#else /* EMLOG_USE_NO_ATOMICS */
#define EMLOG__LOAD_RELAXED(ptr)       (*(ptr))
#define EMLOG__LOAD_ACQUIRE(ptr)       (*(ptr))
#define EMLOG__STORE_RELAXED(ptr, val) ((void)(*(ptr) = (val)))
#define EMLOG__STORE_RELEASE(ptr, val) ((void)(*(ptr) = (val)))
#define EMLOG__FENCE_AFTER_ACQUIRE()   EMLOG_FENCE_ACQUIRE()
#define EMLOG__FENCE_BEFORE_RELEASE()  EMLOG_FENCE_RELEASE()
#endif

#if defined(EMLOG_USE_GNU_ATOMICS)
/* __atomic_always_lock_free is a compile-time constant but not a standard
 * integer constant expression, so -Wpedantic objects to it inside
 * _Static_assert; suppress only that diagnostic. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
_Static_assert(__atomic_always_lock_free(sizeof(uint_least8_t), 0),
               "emlog: uint_least8_t indices are not lock-free on this target");
#pragma GCC diagnostic pop
#elif defined(EMLOG_USE_C11_ATOMICS)
/* uint_least8_t has the size of char; require "always lock-free" (2). */
_Static_assert((sizeof(uint_least8_t) == sizeof(char))
                   && (ATOMIC_CHAR_LOCK_FREE == 2),
               "emlog: uint_least8_t indices are not always lock-free");
#endif

/* Claim the slot for the next formatted entry, evicting the oldest one
 * when the ring is full. */
static struct emlog_entry *
emlog_claim(struct emlog_ctx *ctx)
{
        struct emlog_entry *entry = &ctx->buffer[ctx->head];

        ctx->head++;
        if (ctx->head >= EMLOG_ENTRIES) {
                ctx->head = 0u;
        }
        if (ctx->count < EMLOG_ENTRIES) {
                ctx->count++;
        }
        return entry;
}

/* On an encoding error store an empty, terminated message rather than
 * whatever the formatter left in the buffer. */
static void
emlog_check_written(struct emlog_entry *entry, int written)
{
        if (written < 0) {
                entry->msg[0] = '\0';
        }
}

static uint_least8_t
emlog_isr_next(uint_least8_t idx)
{
        return (idx >= (uint_least8_t)EMLOG_ISR_ENTRIES)
                   ? (uint_least8_t)0u
                   : (uint_least8_t)(idx + 1u);
}

void
emlog_init(struct emlog_ctx *ctx, uint32_t (*timestamp_fn)(void))
{
        if (ctx == NULL) {
                return;
        }
        ctx->head = 0u;
        ctx->count = 0u;
        ctx->timestamp_fn = timestamp_fn;
        (void)memset(ctx->buffer, 0, sizeof(ctx->buffer));
        EMLOG__STORE_RELAXED(&ctx->isr_head, (uint_least8_t)0u);
        EMLOG__STORE_RELAXED(&ctx->isr_tail, (uint_least8_t)0u);
        EMLOG__STORE_RELAXED(&ctx->isr_dropped, (uint_least8_t)0u);
}

void
emlog_event(struct emlog_ctx *ctx, enum emlog_level level, const char *fmt, ...)
{
        if ((ctx == NULL) || (ctx->timestamp_fn == NULL) || (fmt == NULL)) {
                return;
        }

        struct emlog_entry *entry = emlog_claim(ctx);
        entry->timestamp = ctx->timestamp_fn();
        entry->level = (uint16_t)level;

        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(entry->msg, (size_t)EMLOG_MSG_LEN, fmt, args);
        va_end(args);

        emlog_check_written(entry, written);
}

bool
emlog_event_isr(struct emlog_ctx *ctx, enum emlog_level level, const char *fmt,
                uint32_t arg0, uint32_t arg1)
{
        if ((ctx == NULL) || (ctx->timestamp_fn == NULL) || (fmt == NULL)) {
                return false;
        }

        const uint_least8_t head = EMLOG__LOAD_RELAXED(&ctx->isr_head);
        const uint_least8_t tail = EMLOG__LOAD_ACQUIRE(&ctx->isr_tail);
        EMLOG__FENCE_AFTER_ACQUIRE();
        const uint_least8_t next = emlog_isr_next(head);

        if (next == tail) {
                const uint_least8_t dropped =
                    EMLOG__LOAD_RELAXED(&ctx->isr_dropped);
                if (dropped < (uint_least8_t)UINT_LEAST8_MAX) {
                        EMLOG__STORE_RELAXED(&ctx->isr_dropped,
                                             (uint_least8_t)(dropped + 1u));
                }
                return false;
        }

        volatile struct emlog_pending *slot = &ctx->isr_ring[head];
        slot->fmt = fmt;
        slot->timestamp = ctx->timestamp_fn();
        slot->arg[0] = arg0;
        slot->arg[1] = arg1;
        slot->level = (uint16_t)level;

        EMLOG__FENCE_BEFORE_RELEASE();
        EMLOG__STORE_RELEASE(&ctx->isr_head, next);
        return true;
}

uint16_t
emlog_drain(struct emlog_ctx *ctx)
{
        if (ctx == NULL) {
                return 0u;
        }

        uint16_t drained = 0u;
        uint_least8_t tail = EMLOG__LOAD_RELAXED(&ctx->isr_tail);
        const uint_least8_t head = EMLOG__LOAD_ACQUIRE(&ctx->isr_head);
        EMLOG__FENCE_AFTER_ACQUIRE();

        while (tail != head) {
                const volatile struct emlog_pending *slot =
                    &ctx->isr_ring[tail];
                struct emlog_entry *entry = emlog_claim(ctx);
                entry->timestamp = slot->timestamp;
                entry->level = slot->level;
                int written = snprintf(entry->msg, (size_t)EMLOG_MSG_LEN,
                                       slot->fmt, slot->arg[0], slot->arg[1]);
                emlog_check_written(entry, written);

                tail = emlog_isr_next(tail);
                EMLOG__FENCE_BEFORE_RELEASE();
                EMLOG__STORE_RELEASE(&ctx->isr_tail, tail);
                drained++;
        }
        return drained;
}

uint_least8_t
emlog_get_isr_dropped(const struct emlog_ctx *ctx)
{
        if (ctx == NULL) {
                return 0u;
        }
        return EMLOG__LOAD_RELAXED(&ctx->isr_dropped);
}

uint16_t
emlog_get_count(const struct emlog_ctx *ctx)
{
        if (ctx == NULL) {
                return 0u;
        }
        return ctx->count;
}

const struct emlog_entry *
emlog_get_entry(const struct emlog_ctx *ctx, uint16_t idx)
{
        if ((ctx == NULL) || (idx >= ctx->count)) {
                return NULL;
        }
        uint32_t phys_idx = ((uint32_t)ctx->head + (uint32_t)EMLOG_ENTRIES
                             - (uint32_t)ctx->count + (uint32_t)idx)
                            % (uint32_t)EMLOG_ENTRIES;
        return &ctx->buffer[phys_idx];
}

const struct emlog_entry *
emlog_get_buffer(const struct emlog_ctx *ctx, uint16_t *count)
{
        if (ctx == NULL) {
                if (count != NULL) {
                        *count = 0u;
                }
                return NULL;
        }
        if (count != NULL) {
                *count = ctx->count;
        }
        return ctx->buffer;
}
