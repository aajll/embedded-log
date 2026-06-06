#include <stdint.h>

#include "../subprojects/unity/src/unity.h"

#include "../include/log.h"

static uint32_t fake_time = 0;

static uint32_t
fake_timestamp(void)
{
        return fake_time;
}

void
setUp(void)
{
}
void
tearDown(void)
{
}

void
test_log_init_and_event(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        log_event(&ctx, INFO, "Boot %d", 42);
        fake_time += 5;
        log_event(&ctx, FAULT, "Overtemp!");
        fake_time += 5;
        log_event(&ctx, WARN, "Retrying...");

        TEST_ASSERT_EQUAL_UINT16(3, log_get_count(&ctx));

        const struct log_entry *e0 = log_get_entry(&ctx, 0);
        TEST_ASSERT_NOT_NULL(e0);
        TEST_ASSERT_EQUAL_UINT32(0, e0->timestamp);
        TEST_ASSERT_EQUAL_UINT16(INFO, e0->level);
        TEST_ASSERT_EQUAL_STRING("Boot 42", e0->msg);

        const struct log_entry *e1 = log_get_entry(&ctx, 1);
        TEST_ASSERT_NOT_NULL(e1);
        TEST_ASSERT_EQUAL_UINT32(5, e1->timestamp);
        TEST_ASSERT_EQUAL_UINT16(FAULT, e1->level);
        TEST_ASSERT_EQUAL_STRING("Overtemp!", e1->msg);

        const struct log_entry *e2 = log_get_entry(&ctx, 2);
        TEST_ASSERT_NOT_NULL(e2);
        TEST_ASSERT_EQUAL_UINT32(10, e2->timestamp);
        TEST_ASSERT_EQUAL_UINT16(WARN, e2->level);
        TEST_ASSERT_EQUAL_STRING("Retrying...", e2->msg);
}

void
test_log_once_macro(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        for (int i = 0; i < 10; ++i) {
                LOG_ONCE(&ctx, WARN, "Logged only once");
        }
        TEST_ASSERT_EQUAL_UINT16(1, log_get_count(&ctx));
        const struct log_entry *e0 = log_get_entry(&ctx, 0);
        TEST_ASSERT_EQUAL_STRING("Logged only once", e0->msg);
}

void
test_log_get_entry_oob(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        log_event(&ctx, INFO, "test");
        TEST_ASSERT_NULL(
            log_get_entry(&ctx, 2)); // Only 1 entry, so index 2 invalid
        TEST_ASSERT_NULL(log_get_entry(&ctx, 100)); // Large invalid index
}

void
test_log_buffer_wraparound(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        const uint16_t N = LOG_ENTRIES + 5;
        for (uint16_t i = 0; i < N; ++i) {
                log_event(&ctx, INFO, "Entry %u", i);
                fake_time += 1;
        }
        TEST_ASSERT_EQUAL_UINT16(LOG_ENTRIES, log_get_count(&ctx));

        // The oldest retained entry should be N - LOG_ENTRIES
        const struct log_entry *oldest = log_get_entry(&ctx, 0);
        TEST_ASSERT_NOT_NULL(oldest);

        char expected_msg[32];
        snprintf(expected_msg, sizeof(expected_msg), "Entry %u",
                 N - LOG_ENTRIES);
        TEST_ASSERT_EQUAL_STRING(expected_msg, oldest->msg);

        // The newest retained entry should be N-1
        const struct log_entry *newest = log_get_entry(&ctx, LOG_ENTRIES - 1);
        snprintf(expected_msg, sizeof(expected_msg), "Entry %u", N - 1);
        TEST_ASSERT_EQUAL_STRING(expected_msg, newest->msg);
}

void
test_log_buffer_wraparound_timestamps(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        uint32_t start_time = 1000;
        fake_time = start_time;
        const uint16_t N = LOG_ENTRIES + 2;

        for (uint16_t i = 0; i < N; ++i) {
                log_event(&ctx, FAULT, "Overrun %u", i);
                fake_time += 10;
        }
        const struct log_entry *first = log_get_entry(&ctx, 0);
        TEST_ASSERT_NOT_NULL(first);
        TEST_ASSERT_EQUAL_UINT16(FAULT, first->level);
        TEST_ASSERT_EQUAL_UINT32(start_time + 20, first->timestamp);

        const struct log_entry *last = log_get_entry(&ctx, LOG_ENTRIES - 1);
        TEST_ASSERT_NOT_NULL(last);
        TEST_ASSERT_EQUAL_UINT32(start_time + 10 * (N - 1), last->timestamp);
}

void
test_log_once_reset_context(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        void my_state(struct log_ctx * c)
        {
                LOG_ONCE(c, INFO, "Entered state");
        }

        // First entry (should log)
        my_state(&ctx);
        TEST_ASSERT_EQUAL_UINT16(1, log_get_count(&ctx));

        // Second call (should not log)
        my_state(&ctx);
        TEST_ASSERT_EQUAL_UINT16(1, log_get_count(&ctx));
}

void
test_log_event_null_fmt(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        // Should not crash, should not log
        log_event(&ctx, INFO, NULL);
        TEST_ASSERT_EQUAL_UINT16(0, log_get_count(&ctx));
}

void
test_log_init_null_fn(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        log_event(&ctx, INFO, "should log");
        TEST_ASSERT_EQUAL_UINT16(1, log_get_count(&ctx));

        log_init(&ctx, NULL);
        log_event(&ctx, INFO, "should not log");
        TEST_ASSERT_EQUAL_UINT16(0, log_get_count(&ctx));
}

void
test_log_get_buffer_returns_correct_count_and_pointer(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);

        uint16_t count = 12345; // sentinel
        const struct log_entry *buf = log_get_buffer(&ctx, &count);

        TEST_ASSERT_NOT_NULL(buf);
        TEST_ASSERT_EQUAL_UINT16(0, count);

        log_event(&ctx, INFO, "System start");
        log_event(&ctx, FAULT, "Fault detected");

        count = 0;
        buf = log_get_buffer(&ctx, &count);

        TEST_ASSERT_EQUAL_UINT16(2, count);
        TEST_ASSERT_EQUAL_STRING("System start", buf[0].msg);
        TEST_ASSERT_EQUAL_STRING("Fault detected", buf[1].msg);

        const struct log_entry *e0 = log_get_entry(&ctx, 0);
        const struct log_entry *e1 = log_get_entry(&ctx, 1);
        TEST_ASSERT_EQUAL_STRING(e0->msg, buf[0].msg);
        TEST_ASSERT_EQUAL_STRING(e1->msg, buf[1].msg);
}

void
test_log_get_buffer_null_count_ptr(void)
{
        struct log_ctx ctx;
        log_init(&ctx, fake_timestamp);
        log_event(&ctx, WARN, "Hello world");

        const struct log_entry *buf = log_get_buffer(&ctx, NULL);

        TEST_ASSERT_NOT_NULL(buf);
        TEST_ASSERT_EQUAL_STRING("Hello world", buf[0].msg);
}

int
main(void)
{
        UNITY_BEGIN();
        RUN_TEST(test_log_init_and_event);
        RUN_TEST(test_log_once_macro);
        RUN_TEST(test_log_get_entry_oob);
        RUN_TEST(test_log_buffer_wraparound);
        RUN_TEST(test_log_buffer_wraparound_timestamps);
        RUN_TEST(test_log_once_reset_context);
        RUN_TEST(test_log_event_null_fmt);
        RUN_TEST(test_log_init_null_fn);
        RUN_TEST(test_log_get_buffer_returns_correct_count_and_pointer);
        RUN_TEST(test_log_get_buffer_null_count_ptr);
        return UNITY_END();
}
