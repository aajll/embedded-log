/*
 * SPDX-License-Identifier: MIT
 *
 * @file: test_event.c
 * @brief Recording entries: formatting, levels, timestamps, truncation and
 *        EMLOG_ONCE.
 */

#include <stdint.h>
#include <string.h>
#include <wchar.h>

#include "test_harness.h"

#include "emlog.h"

static uint32_t fake_time = 0u;

static uint32_t
fake_timestamp(void)
{
        return fake_time;
}

/* One EMLOG_ONCE call site reached repeatedly, as a state-machine step
 * function would be. */
static void
my_state(struct emlog_ctx *c)
{
        EMLOG_ONCE(c, EMLOG_INFO, "Entered state");
}

TEST_CASE(test_event_records_level_timestamp_and_message)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        fake_time = 0u;
        emlog_event(&ctx, EMLOG_INFO, "Boot %d", 42);
        fake_time = 5u;
        emlog_event(&ctx, EMLOG_FAULT, "Overtemp!");
        fake_time = 10u;
        emlog_event(&ctx, EMLOG_WARN, "Retrying...");

        TEST_ASSERT(emlog_get_count(&ctx) == 3u);

        const struct emlog_entry *e0 = emlog_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(e0->timestamp == 0u);
        TEST_ASSERT(e0->level == (uint16_t)EMLOG_INFO);
        TEST_ASSERT(strcmp(e0->msg, "Boot 42") == 0);

        const struct emlog_entry *e1 = emlog_get_entry(&ctx, 1u);
        TEST_ASSERT(e1 != NULL);
        TEST_ASSERT(e1->timestamp == 5u);
        TEST_ASSERT(e1->level == (uint16_t)EMLOG_FAULT);
        TEST_ASSERT(strcmp(e1->msg, "Overtemp!") == 0);

        const struct emlog_entry *e2 = emlog_get_entry(&ctx, 2u);
        TEST_ASSERT(e2 != NULL);
        TEST_ASSERT(e2->timestamp == 10u);
        TEST_ASSERT(e2->level == (uint16_t)EMLOG_WARN);
        TEST_ASSERT(strcmp(e2->msg, "Retrying...") == 0);
}

TEST_CASE(test_event_truncates_and_terminates)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        char long_msg[EMLOG_MSG_LEN + 16u];
        (void)memset(long_msg, 'A', sizeof(long_msg) - 1u);
        long_msg[sizeof(long_msg) - 1u] = '\0';

        emlog_event(&ctx, EMLOG_INFO, "%s", long_msg);

        const struct emlog_entry *e0 = emlog_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(strlen(e0->msg) == (size_t)(EMLOG_MSG_LEN - 1u));
        TEST_ASSERT(e0->msg[EMLOG_MSG_LEN - 1u] == '\0');
}

TEST_CASE(test_event_encoding_error_stores_empty_message)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        /* U+20AC has no encoding in the "C" locale, so the conversion
         * fails and vsnprintf returns a negative value after writing the
         * "partial" prefix. */
        emlog_event(&ctx, EMLOG_FAULT, "partial%lc", (wint_t)0x20AC);

        const struct emlog_entry *e0 = emlog_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(e0->level == (uint16_t)EMLOG_FAULT);
        TEST_ASSERT(e0->msg[0] == '\0');
}

TEST_CASE(test_event_null_fmt_is_ignored)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);
        emlog_event(&ctx, EMLOG_INFO, NULL);
        TEST_ASSERT(emlog_get_count(&ctx) == 0u);
}

TEST_CASE(test_once_logs_once_per_site)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        for (int i = 0; i < 10; ++i) {
                EMLOG_ONCE(&ctx, EMLOG_WARN, "Attempt %d of %d", i, 10);
        }
        TEST_ASSERT(emlog_get_count(&ctx) == 1u);
        const struct emlog_entry *e0 = emlog_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(strcmp(e0->msg, "Attempt 0 of 10") == 0);

        my_state(&ctx);
        my_state(&ctx);
        TEST_ASSERT(emlog_get_count(&ctx) == 2u);
}

int
main(void)
{
        run_test(test_event_records_level_timestamp_and_message,
                 "test_event_records_level_timestamp_and_message");
        run_test(test_event_truncates_and_terminates,
                 "test_event_truncates_and_terminates");
        run_test(test_event_encoding_error_stores_empty_message,
                 "test_event_encoding_error_stores_empty_message");
        run_test(test_event_null_fmt_is_ignored,
                 "test_event_null_fmt_is_ignored");
        run_test(test_once_logs_once_per_site, "test_once_logs_once_per_site");
        return EXIT_SUCCESS;
}
