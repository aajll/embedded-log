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
 *      emlog_event API requires va_list and vsnprintf. vsnprintf is
 *      bounded and the result is always NUL-terminated.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "emlog.h"

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
}

void
emlog_event(struct emlog_ctx *ctx, enum emlog_level level, const char *fmt, ...)
{
        if ((ctx == NULL) || (ctx->timestamp_fn == NULL) || (fmt == NULL)) {
                return;
        }

        struct emlog_entry *entry = &ctx->buffer[ctx->head];
        entry->timestamp = ctx->timestamp_fn();
        entry->level = (uint16_t)level;

        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(entry->msg, (size_t)EMLOG_MSG_LEN, fmt, args);
        va_end(args);

        if (written < 0) {
                /* Encoding error: store an empty, terminated message
                 * rather than indeterminate buffer contents. */
                entry->msg[0] = '\0';
        }

        ctx->head++;
        if (ctx->head >= EMLOG_ENTRIES) {
                ctx->head = 0u;
        }
        if (ctx->count < EMLOG_ENTRIES) {
                ctx->count++;
        }
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
