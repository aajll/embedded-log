/*
 * SPDX-License-Identifier: MIT
 *
 * @file: test_read.c
 * @brief Reading the ring: chronological access, bounds, wrap-around and
 *        the physical-order buffer view.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_harness.h"

#include "emlog.h"

static uint32_t fake_time = 0u;

static uint32_t
fake_timestamp(void)
{
        return fake_time;
}

static void
expect_msg(const struct emlog_entry *e, unsigned n)
{
        char expected[32];
        (void)snprintf(expected, sizeof(expected), "Entry %u", n);
        TEST_ASSERT(e != NULL);
        TEST_ASSERT(strcmp(e->msg, expected) == 0);
}

TEST_CASE(test_get_entry_out_of_bounds)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);
        TEST_ASSERT(emlog_get_entry(&ctx, 0u) == NULL);

        emlog_event(&ctx, EMLOG_INFO, "test");
        TEST_ASSERT(emlog_get_entry(&ctx, 0u) != NULL);
        TEST_ASSERT(emlog_get_entry(&ctx, 1u) == NULL);
        TEST_ASSERT(emlog_get_entry(&ctx, 100u) == NULL);
}

TEST_CASE(test_wraparound_keeps_newest)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        const uint32_t start_time = 1000u;
        fake_time = start_time;
        const uint16_t n = EMLOG_ENTRIES + 5u;
        for (uint16_t i = 0u; i < n; ++i) {
                emlog_event(&ctx, EMLOG_INFO, "Entry %u", (unsigned)i);
                fake_time += 10u;
        }
        TEST_ASSERT(emlog_get_count(&ctx) == EMLOG_ENTRIES);

        const struct emlog_entry *oldest = emlog_get_entry(&ctx, 0u);
        expect_msg(oldest, (unsigned)(n - EMLOG_ENTRIES));
        TEST_ASSERT(oldest->timestamp == start_time + (10u * 5u));

        const struct emlog_entry *newest =
            emlog_get_entry(&ctx, EMLOG_ENTRIES - 1u);
        expect_msg(newest, (unsigned)(n - 1u));
        TEST_ASSERT(newest->timestamp == start_time + (10u * (n - 1u)));
        TEST_ASSERT(emlog_get_entry(&ctx, EMLOG_ENTRIES) == NULL);
}

TEST_CASE(test_get_buffer_storage_order)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        uint16_t count = 12345u;
        const struct emlog_entry *buf = emlog_get_buffer(&ctx, &count);
        TEST_ASSERT(buf != NULL);
        TEST_ASSERT(count == 0u);

        emlog_event(&ctx, EMLOG_INFO, "System start");
        emlog_event(&ctx, EMLOG_FAULT, "Fault detected");

        buf = emlog_get_buffer(&ctx, &count);
        TEST_ASSERT(count == 2u);
        TEST_ASSERT(buf == emlog_get_entry(&ctx, 0u));
        TEST_ASSERT(strcmp(buf[0].msg, "System start") == 0);
        TEST_ASSERT(strcmp(buf[1].msg, "Fault detected") == 0);

        TEST_ASSERT(emlog_get_buffer(&ctx, NULL) == buf);
}

TEST_CASE(test_get_buffer_physical_order_after_wrap)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        /* Wrap the ring by one entry: slot 0 then holds the newest entry,
         * not the oldest. */
        const uint16_t n = EMLOG_ENTRIES + 1u;
        for (uint16_t i = 0u; i < n; ++i) {
                emlog_event(&ctx, EMLOG_INFO, "Entry %u", (unsigned)i);
        }

        uint16_t count = 0u;
        const struct emlog_entry *buf = emlog_get_buffer(&ctx, &count);
        TEST_ASSERT(count == EMLOG_ENTRIES);
        expect_msg(&buf[0], (unsigned)(n - 1u));
        expect_msg(emlog_get_entry(&ctx, 0u), 1u);
}

int
main(void)
{
        run_test(test_get_entry_out_of_bounds, "test_get_entry_out_of_bounds");
        run_test(test_wraparound_keeps_newest, "test_wraparound_keeps_newest");
        run_test(test_get_buffer_storage_order,
                 "test_get_buffer_storage_order");
        run_test(test_get_buffer_physical_order_after_wrap,
                 "test_get_buffer_physical_order_after_wrap");
        return EXIT_SUCCESS;
}
