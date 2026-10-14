/*
 * SPDX-License-Identifier: MIT
 *
 * @file: test_isr.c
 * @brief Deferred interrupt path: emlog_event_isr(), emlog_drain() and
 *        emlog_get_isr_dropped(), built once per atomic backend.
 */

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "test_harness.h"

#include "emlog.h"

#if !defined(EMLOG_USE_NO_ATOMICS)
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#endif

static uint32_t fake_time = 0u;

static uint32_t
fake_timestamp(void)
{
        return fake_time;
}

TEST_CASE(test_isr_defers_formatting_until_drain)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        fake_time = 7u;
        TEST_ASSERT(emlog_event_isr(
            &ctx, EMLOG_FAULT, "adc %" PRIu32 " ch %" PRIx32, 4095u, 0xAu));
        TEST_ASSERT(emlog_get_count(&ctx) == 0u);

        fake_time = 99u;
        TEST_ASSERT(emlog_drain(&ctx) == 1u);
        TEST_ASSERT(emlog_get_count(&ctx) == 1u);

        const struct emlog_entry *e = emlog_get_entry(&ctx, 0u);
        TEST_ASSERT(e != NULL);
        TEST_ASSERT(e->timestamp == 7u);
        TEST_ASSERT(e->level == (uint16_t)EMLOG_FAULT);
        TEST_ASSERT(strcmp(e->msg, "adc 4095 ch a") == 0);

        TEST_ASSERT(emlog_drain(&ctx) == 0u);
        TEST_ASSERT(emlog_get_count(&ctx) == 1u);
        TEST_PASS("test_isr_defers_formatting_until_drain");
}

TEST_CASE(test_isr_drain_appends_in_posting_order)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        fake_time = 1u;
        emlog_event(&ctx, EMLOG_INFO, "task");
        for (uint32_t i = 0u; i < 3u; i++) {
                fake_time = 10u + i;
                TEST_ASSERT(
                    emlog_event_isr(&ctx, EMLOG_WARN, "isr %" PRIu32, i, 0u));
        }
        TEST_ASSERT(emlog_drain(&ctx) == 3u);
        TEST_ASSERT(emlog_get_count(&ctx) == 4u);

        TEST_ASSERT(strcmp(emlog_get_entry(&ctx, 0u)->msg, "task") == 0);
        TEST_ASSERT(strcmp(emlog_get_entry(&ctx, 1u)->msg, "isr 0") == 0);
        TEST_ASSERT(strcmp(emlog_get_entry(&ctx, 2u)->msg, "isr 1") == 0);
        TEST_ASSERT(strcmp(emlog_get_entry(&ctx, 3u)->msg, "isr 2") == 0);
        TEST_ASSERT(emlog_get_entry(&ctx, 3u)->timestamp == 12u);
        TEST_PASS("test_isr_drain_appends_in_posting_order");
}

/* The pending ring holds exactly EMLOG_ISR_ENTRIES; the next post is
 * rejected and counted, and draining frees the slots again across the
 * index wrap. */
TEST_CASE(test_isr_full_ring_drops_and_counts)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        for (uint32_t round = 0u; round < 3u; round++) {
                for (uint32_t i = 0u; i < EMLOG_ISR_ENTRIES; i++) {
                        TEST_ASSERT(emlog_event_isr(&ctx, EMLOG_INFO,
                                                    "%" PRIu32 ".%" PRIu32,
                                                    round, i));
                }
                TEST_ASSERT(!emlog_event_isr(&ctx, EMLOG_INFO, "lost", 0u, 0u));
                TEST_ASSERT(emlog_get_isr_dropped(&ctx)
                            == (uint_least8_t)(round + 1u));
                TEST_ASSERT(emlog_drain(&ctx) == EMLOG_ISR_ENTRIES);

                char want[16];
                (void)snprintf(want, sizeof(want), "%" PRIu32 ".%u", round,
                               EMLOG_ISR_ENTRIES - 1u);
                uint16_t last = (uint16_t)(emlog_get_count(&ctx) - 1u);
                TEST_ASSERT(strcmp(emlog_get_entry(&ctx, last)->msg, want)
                            == 0);
        }
        TEST_PASS("test_isr_full_ring_drops_and_counts");
}

TEST_CASE(test_isr_dropped_counter_saturates)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        for (uint32_t i = 0u; i < EMLOG_ISR_ENTRIES; i++) {
                TEST_ASSERT(emlog_event_isr(&ctx, EMLOG_INFO, "x", 0u, 0u));
        }
        for (uint32_t i = 0u; i < (uint32_t)UINT_LEAST8_MAX + 5u; i++) {
                TEST_ASSERT(!emlog_event_isr(&ctx, EMLOG_INFO, "x", 0u, 0u));
        }
        TEST_ASSERT(emlog_get_isr_dropped(&ctx) == UINT_LEAST8_MAX);

        emlog_init(&ctx, fake_timestamp);
        TEST_ASSERT(emlog_get_isr_dropped(&ctx) == 0u);
        TEST_ASSERT(emlog_drain(&ctx) == 0u);
        TEST_PASS("test_isr_dropped_counter_saturates");
}

TEST_CASE(test_isr_drain_encoding_error_stores_empty_message)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        /* "%lc" consumes a wint_t; 0x20AC is not representable in the C
         * locale, so snprintf reports an encoding error. */
        TEST_ASSERT(
            emlog_event_isr(&ctx, EMLOG_INFO, "partial%lc", 0x20ACu, 0u));
        TEST_ASSERT(emlog_drain(&ctx) == 1u);
        TEST_ASSERT(emlog_get_entry(&ctx, 0u)->msg[0] == '\0');
        TEST_PASS("test_isr_drain_encoding_error_stores_empty_message");
}

TEST_CASE(test_isr_rejects_invalid_arguments)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        TEST_ASSERT(!emlog_event_isr(NULL, EMLOG_INFO, "x", 0u, 0u));
        TEST_ASSERT(!emlog_event_isr(&ctx, EMLOG_INFO, NULL, 0u, 0u));
        TEST_ASSERT(emlog_drain(NULL) == 0u);
        TEST_ASSERT(emlog_get_isr_dropped(NULL) == 0u);

        struct emlog_ctx no_clock;
        emlog_init(&no_clock, NULL);
        TEST_ASSERT(!emlog_event_isr(&no_clock, EMLOG_INFO, "x", 0u, 0u));

        /* Rejected arguments are not ring overflow. */
        TEST_ASSERT(emlog_get_isr_dropped(&ctx) == 0u);
        TEST_ASSERT(emlog_get_isr_dropped(&no_clock) == 0u);
        TEST_ASSERT(emlog_drain(&ctx) == 0u);
        TEST_PASS("test_isr_rejects_invalid_arguments");
}

#if !defined(EMLOG_USE_NO_ATOMICS)
/* One producer thread stands in for the interrupt; the main thread drains
 * concurrently. Every accepted entry must arrive once, in order, intact. */
#define MT_POSTS 200000u

static struct emlog_ctx mt_ctx;
static atomic_bool mt_done;

static uint32_t
mt_timestamp(void)
{
        return 0u;
}

static void *
mt_producer(void *arg)
{
        uint32_t *accepted = arg;
        for (uint32_t i = 0u; i < MT_POSTS; i++) {
                if (emlog_event_isr(&mt_ctx, EMLOG_INFO, "%" PRIu32 ":%" PRIu32,
                                    i, ~i)) {
                        (*accepted)++;
                }
        }
        atomic_store(&mt_done, true);
        return NULL;
}

/* Check the newest `n` entries just drained and return the last sequence
 * number seen. */
static uint32_t
mt_check(uint16_t n, uint32_t prev, bool have_prev)
{
        uint16_t count = emlog_get_count(&mt_ctx);
        for (uint16_t k = (uint16_t)(count - n); k < count; k++) {
                const struct emlog_entry *e = emlog_get_entry(&mt_ctx, k);
                uint32_t seq = 0u;
                uint32_t inv = 0u;
                TEST_ASSERT(sscanf(e->msg, "%" SCNu32 ":%" SCNu32, &seq, &inv)
                            == 2);
                TEST_ASSERT(inv == ~seq);
                TEST_ASSERT(!have_prev || (seq > prev));
                prev = seq;
                have_prev = true;
        }
        return prev;
}

TEST_CASE(test_isr_concurrent_producer_and_drain)
{
        emlog_init(&mt_ctx, mt_timestamp);
        atomic_store(&mt_done, false);
        uint32_t accepted = 0u;
        pthread_t producer;
        TEST_ASSERT(pthread_create(&producer, NULL, mt_producer, &accepted)
                    == 0);

        uint32_t drained = 0u;
        uint32_t prev = 0u;
        bool have_prev = false;
        bool done = false;
        while (!done) {
                /* Read the flag before draining, so the final pass sees
                 * every entry the producer published. */
                done = atomic_load(&mt_done);
                uint16_t n;
                do {
                        /* Each drain moves at most EMLOG_ISR_ENTRIES, which
                         * fits the main ring, so a batch is checked before
                         * it can be overwritten. */
                        n = emlog_drain(&mt_ctx);
                        if (n > 0u) {
                                prev = mt_check(n, prev, have_prev);
                                have_prev = true;
                                drained += n;
                        }
                } while (n > 0u);
        }
        TEST_ASSERT(pthread_join(producer, NULL) == 0);
        TEST_ASSERT(accepted > 0u);
        TEST_ASSERT(drained == accepted);
        TEST_PASS("test_isr_concurrent_producer_and_drain");
}
#endif

int
main(void)
{
        run_test(test_isr_defers_formatting_until_drain,
                 "test_isr_defers_formatting_until_drain");
        run_test(test_isr_drain_appends_in_posting_order,
                 "test_isr_drain_appends_in_posting_order");
        run_test(test_isr_full_ring_drops_and_counts,
                 "test_isr_full_ring_drops_and_counts");
        run_test(test_isr_dropped_counter_saturates,
                 "test_isr_dropped_counter_saturates");
        run_test(test_isr_drain_encoding_error_stores_empty_message,
                 "test_isr_drain_encoding_error_stores_empty_message");
        run_test(test_isr_rejects_invalid_arguments,
                 "test_isr_rejects_invalid_arguments");
#if !defined(EMLOG_USE_NO_ATOMICS)
        run_test(test_isr_concurrent_producer_and_drain,
                 "test_isr_concurrent_producer_and_drain");
#endif
        return 0;
}
