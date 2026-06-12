/*
 * @file: test_log.c
 * @brief Unit tests for the embedded logging facility.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_harness.h"

#include "log.h"

static uint32_t fake_time = 0u;

static uint32_t
fake_timestamp(void)
{
        return fake_time;
}

/* Helper exercising LOG_ONCE from a single call site, as a state-machine
 * step function would. */
static void
my_state(struct log_ctx *c)
{
        LOG_ONCE(c, LOG_INFO, "Entered state");
}

TEST_CASE(test_log_init_and_event)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        fake_time = 0u;
        log_event(&ctx, LOG_INFO, "Boot %d", 42);
        fake_time += 5u;
        log_event(&ctx, LOG_FAULT, "Overtemp!");
        fake_time += 5u;
        log_event(&ctx, LOG_WARN, "Retrying...");

        TEST_ASSERT(log_get_count(&ctx) == 3u);

        const struct log_entry *e0 = log_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(e0->timestamp == 0u);
        TEST_ASSERT(e0->level == (uint16_t)LOG_INFO);
        TEST_ASSERT(strcmp(e0->msg, "Boot 42") == 0);

        const struct log_entry *e1 = log_get_entry(&ctx, 1u);
        TEST_ASSERT(e1 != NULL);
        TEST_ASSERT(e1->timestamp == 5u);
        TEST_ASSERT(e1->level == (uint16_t)LOG_FAULT);
        TEST_ASSERT(strcmp(e1->msg, "Overtemp!") == 0);

        const struct log_entry *e2 = log_get_entry(&ctx, 2u);
        TEST_ASSERT(e2 != NULL);
        TEST_ASSERT(e2->timestamp == 10u);
        TEST_ASSERT(e2->level == (uint16_t)LOG_WARN);
        TEST_ASSERT(strcmp(e2->msg, "Retrying...") == 0);
}

TEST_CASE(test_log_once_macro)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        for (int i = 0; i < 10; ++i) {
                LOG_ONCE(&ctx, LOG_WARN, "Logged only once");
        }
        TEST_ASSERT(log_get_count(&ctx) == 1u);
        const struct log_entry *e0 = log_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(strcmp(e0->msg, "Logged only once") == 0);
}

TEST_CASE(test_log_once_with_format_args)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        for (int i = 0; i < 5; ++i) {
                LOG_ONCE(&ctx, LOG_WARN, "Attempt %d of %d", i, 5);
        }
        TEST_ASSERT(log_get_count(&ctx) == 1u);
        const struct log_entry *e0 = log_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(strcmp(e0->msg, "Attempt 0 of 5") == 0);
}

TEST_CASE(test_log_get_entry_oob)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        log_event(&ctx, LOG_INFO, "test");
        /* Only 1 entry, so index 2 invalid. */
        TEST_ASSERT(log_get_entry(&ctx, 2u) == NULL);
        /* Large invalid index. */
        TEST_ASSERT(log_get_entry(&ctx, 100u) == NULL);
}

TEST_CASE(test_log_buffer_wraparound)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        const uint16_t N = LOG_ENTRIES + 5u;
        for (uint16_t i = 0u; i < N; ++i) {
                log_event(&ctx, LOG_INFO, "Entry %u", i);
                fake_time += 1u;
        }
        TEST_ASSERT(log_get_count(&ctx) == LOG_ENTRIES);

        /* The oldest retained entry should be N - LOG_ENTRIES. */
        const struct log_entry *oldest = log_get_entry(&ctx, 0u);
        TEST_ASSERT(oldest != NULL);

        char expected_msg[32];
        (void)snprintf(expected_msg, sizeof(expected_msg), "Entry %u",
                       (unsigned)(N - LOG_ENTRIES));
        TEST_ASSERT(strcmp(oldest->msg, expected_msg) == 0);

        /* The newest retained entry should be N - 1. */
        const struct log_entry *newest = log_get_entry(&ctx, LOG_ENTRIES - 1u);
        TEST_ASSERT(newest != NULL);
        (void)snprintf(expected_msg, sizeof(expected_msg), "Entry %u",
                       (unsigned)(N - 1u));
        TEST_ASSERT(strcmp(newest->msg, expected_msg) == 0);
}

TEST_CASE(test_log_buffer_wraparound_timestamps)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        const uint32_t start_time = 1000u;
        fake_time = start_time;
        const uint16_t N = LOG_ENTRIES + 2u;

        for (uint16_t i = 0u; i < N; ++i) {
                log_event(&ctx, LOG_FAULT, "Overrun %u", i);
                fake_time += 10u;
        }
        const struct log_entry *first = log_get_entry(&ctx, 0u);
        TEST_ASSERT(first != NULL);
        TEST_ASSERT(first->level == (uint16_t)LOG_FAULT);
        TEST_ASSERT(first->timestamp == (start_time + 20u));

        const struct log_entry *last = log_get_entry(&ctx, LOG_ENTRIES - 1u);
        TEST_ASSERT(last != NULL);
        TEST_ASSERT(last->timestamp == (start_time + (10u * (N - 1u))));
}

TEST_CASE(test_log_message_truncation)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        char long_msg[LOG_MSG_LEN + 16u];
        (void)memset(long_msg, 'A', sizeof(long_msg) - 1u);
        long_msg[sizeof(long_msg) - 1u] = '\0';

        log_event(&ctx, LOG_INFO, "%s", long_msg);

        const struct log_entry *e0 = log_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        /* Message must be truncated and NUL-terminated. */
        TEST_ASSERT(strlen(e0->msg) == (size_t)(LOG_MSG_LEN - 1u));
        TEST_ASSERT(e0->msg[LOG_MSG_LEN - 1u] == '\0');
}

TEST_CASE(test_log_once_single_call_site)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        /* First entry (should log). */
        my_state(&ctx);
        TEST_ASSERT(log_get_count(&ctx) == 1u);

        /* Second call (should not log). */
        my_state(&ctx);
        TEST_ASSERT(log_get_count(&ctx) == 1u);
}

TEST_CASE(test_log_event_null_fmt)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        /* Should not crash, should not log. */
        log_event(&ctx, LOG_INFO, NULL);
        TEST_ASSERT(log_get_count(&ctx) == 0u);
}

TEST_CASE(test_log_null_ctx)
{
        /* All public functions must tolerate a NULL context. */
        uint16_t count = 123u;

        log_init(NULL, fake_timestamp);
        log_event(NULL, LOG_INFO, "ignored");
        TEST_ASSERT(log_get_count(NULL) == 0u);
        TEST_ASSERT(log_get_entry(NULL, 0u) == NULL);
        TEST_ASSERT(log_get_buffer(NULL, &count) == NULL);
        TEST_ASSERT(count == 0u);
}

TEST_CASE(test_log_init_null_fn)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        log_event(&ctx, LOG_INFO, "should log");
        TEST_ASSERT(log_get_count(&ctx) == 1u);

        log_init(&ctx, NULL);
        log_event(&ctx, LOG_INFO, "should not log");
        TEST_ASSERT(log_get_count(&ctx) == 0u);
}

TEST_CASE(test_log_get_buffer_returns_correct_count_and_pointer)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        uint16_t count = 12345u; /* sentinel */
        const struct log_entry *buf = log_get_buffer(&ctx, &count);

        TEST_ASSERT(buf != NULL);
        TEST_ASSERT(count == 0u);

        log_event(&ctx, LOG_INFO, "System start");
        log_event(&ctx, LOG_FAULT, "Fault detected");

        count = 0u;
        buf = log_get_buffer(&ctx, &count);

        TEST_ASSERT(count == 2u);
        TEST_ASSERT(strcmp(buf[0].msg, "System start") == 0);
        TEST_ASSERT(strcmp(buf[1].msg, "Fault detected") == 0);

        const struct log_entry *e0 = log_get_entry(&ctx, 0u);
        const struct log_entry *e1 = log_get_entry(&ctx, 1u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(e1 != NULL);
        TEST_ASSERT(strcmp(buf[0].msg, e0->msg) == 0);
        TEST_ASSERT(strcmp(buf[1].msg, e1->msg) == 0);
}

TEST_CASE(test_log_get_buffer_null_count_ptr)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        log_event(&ctx, LOG_WARN, "Hello world");

        const struct log_entry *buf = log_get_buffer(&ctx, NULL);

        TEST_ASSERT(buf != NULL);
        TEST_ASSERT(strcmp(buf[0].msg, "Hello world") == 0);
}

TEST_CASE(test_log_get_buffer_physical_order_after_wrap)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        /* Wrap the ring by one entry: slot 0 then holds the newest
         * entry, not the oldest. log_get_buffer exposes physical order;
         * log_get_entry is the chronological accessor. */
        const uint16_t N = LOG_ENTRIES + 1u;
        for (uint16_t i = 0u; i < N; ++i) {
                log_event(&ctx, LOG_INFO, "Entry %u", i);
        }

        uint16_t count = 0u;
        const struct log_entry *buf = log_get_buffer(&ctx, &count);
        TEST_ASSERT(buf != NULL);
        TEST_ASSERT(count == LOG_ENTRIES);

        char expected_msg[32];
        (void)snprintf(expected_msg, sizeof(expected_msg), "Entry %u",
                       (unsigned)(N - 1u));
        TEST_ASSERT(strcmp(buf[0].msg, expected_msg) == 0);

        const struct log_entry *oldest = log_get_entry(&ctx, 0u);
        TEST_ASSERT(oldest != NULL);
        TEST_ASSERT(strcmp(oldest->msg, "Entry 1") == 0);
}

int
main(void)
{
        run_test(test_log_init_and_event, "test_log_init_and_event");
        run_test(test_log_once_macro, "test_log_once_macro");
        run_test(test_log_once_with_format_args,
                 "test_log_once_with_format_args");
        run_test(test_log_get_entry_oob, "test_log_get_entry_oob");
        run_test(test_log_buffer_wraparound, "test_log_buffer_wraparound");
        run_test(test_log_buffer_wraparound_timestamps,
                 "test_log_buffer_wraparound_timestamps");
        run_test(test_log_message_truncation, "test_log_message_truncation");
        run_test(test_log_once_single_call_site,
                 "test_log_once_single_call_site");
        run_test(test_log_event_null_fmt, "test_log_event_null_fmt");
        run_test(test_log_null_ctx, "test_log_null_ctx");
        run_test(test_log_init_null_fn, "test_log_init_null_fn");
        run_test(test_log_get_buffer_returns_correct_count_and_pointer,
                 "test_log_get_buffer_returns_correct_count_and_pointer");
        run_test(test_log_get_buffer_null_count_ptr,
                 "test_log_get_buffer_null_count_ptr");
        run_test(test_log_get_buffer_physical_order_after_wrap,
                 "test_log_get_buffer_physical_order_after_wrap");
        return EXIT_SUCCESS;
}
