/*
 * SPDX-License-Identifier: MIT
 *
 * @file: test_legacy.c
 * @brief Code written against the deprecated log.h names keeps compiling
 *        and behaving the same through the forwarding header.
 */

#include <stdint.h>
#include <string.h>

#include "test_harness.h"

#include "log.h"

static uint32_t
fake_timestamp(void)
{
        return 3u;
}

static void LOG_PRINTF_LIKE(2, 3)
    wrapped(struct log_ctx *c, const char *fmt, ...);

static void
wrapped(struct log_ctx *c, const char *fmt, ...)
{
        (void)fmt;
        log_event(c, LOG_FAULT, "wrapped");
}

TEST_CASE(test_legacy_names)
{
        struct log_ctx ctx;
        enum log_level level = LOG_WARN;
        uint16_t count = 0u;

        log_init(&ctx, fake_timestamp);
        log_event(&ctx, LOG_INFO, "Boot %d", 42);
        log_event(&ctx, level, "warn");
        for (int i = 0; i < 3; ++i) {
                LOG_ONCE(&ctx, LOG_FAULT, "once %d", i);
        }
        wrapped(&ctx, "%d", 1);

        TEST_ASSERT(log_get_count(&ctx) == 4u);

        const struct log_entry *e0 = log_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(e0->timestamp == 3u);
        TEST_ASSERT(e0->level == (uint16_t)LOG_INFO);
        TEST_ASSERT(strcmp(e0->msg, "Boot 42") == 0);

        const struct log_entry *e1 = log_get_entry(&ctx, 1u);
        TEST_ASSERT(e1 != NULL);
        TEST_ASSERT(e1->level == (uint16_t)LOG_WARN);

        const struct log_entry *e2 = log_get_entry(&ctx, 2u);
        TEST_ASSERT(e2 != NULL);
        TEST_ASSERT(strcmp(e2->msg, "once 0") == 0);

        const struct log_entry *buf = log_get_buffer(&ctx, &count);
        TEST_ASSERT(buf == e0);
        TEST_ASSERT(count == 4u);
        TEST_ASSERT(strcmp(buf[3].msg, "wrapped") == 0);
        TEST_ASSERT(sizeof(buf->msg) == LOG_MSG_LEN);
        TEST_ASSERT(sizeof(ctx.buffer) / sizeof(ctx.buffer[0]) == LOG_ENTRIES);
}

int
main(void)
{
        run_test(test_legacy_names, "test_legacy_names");
        return EXIT_SUCCESS;
}
