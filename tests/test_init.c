/*
 * SPDX-License-Identifier: MIT
 *
 * @file: test_init.c
 * @brief Context initialisation and NULL handling.
 */

#include <stdint.h>
#include <string.h>

#include "test_harness.h"

#include "emlog.h"

static uint32_t
fake_timestamp(void)
{
        return 0u;
}

TEST_CASE(test_init_clears_state)
{
        struct emlog_ctx ctx;
        (void)memset(&ctx, 0xA5, sizeof(ctx));

        emlog_init(&ctx, fake_timestamp);

        TEST_ASSERT(ctx.head == 0u);
        TEST_ASSERT(emlog_get_count(&ctx) == 0u);
        TEST_ASSERT(ctx.timestamp_fn == fake_timestamp);
        TEST_ASSERT(ctx.buffer[EMLOG_ENTRIES - 1u].msg[0] == '\0');
}

TEST_CASE(test_reinit_discards_entries)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);
        emlog_event(&ctx, EMLOG_INFO, "first");
        TEST_ASSERT(emlog_get_count(&ctx) == 1u);

        emlog_init(&ctx, fake_timestamp);
        TEST_ASSERT(emlog_get_count(&ctx) == 0u);
        TEST_ASSERT(emlog_get_entry(&ctx, 0u) == NULL);
}

TEST_CASE(test_init_null_timestamp_disables_logging)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, NULL);
        emlog_event(&ctx, EMLOG_INFO, "should not log");
        TEST_ASSERT(emlog_get_count(&ctx) == 0u);
}

TEST_CASE(test_null_ctx)
{
        uint16_t count = 123u;

        emlog_init(NULL, fake_timestamp);
        emlog_event(NULL, EMLOG_INFO, "ignored");
        TEST_ASSERT(emlog_get_count(NULL) == 0u);
        TEST_ASSERT(emlog_get_entry(NULL, 0u) == NULL);
        TEST_ASSERT(emlog_get_buffer(NULL, &count) == NULL);
        TEST_ASSERT(count == 0u);
        TEST_ASSERT(emlog_get_buffer(NULL, NULL) == NULL);
}

int
main(void)
{
        run_test(test_init_clears_state, "test_init_clears_state");
        run_test(test_reinit_discards_entries, "test_reinit_discards_entries");
        run_test(test_init_null_timestamp_disables_logging,
                 "test_init_null_timestamp_disables_logging");
        run_test(test_null_ctx, "test_null_ctx");
        return EXIT_SUCCESS;
}
