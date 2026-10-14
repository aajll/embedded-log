/*
 * SPDX-License-Identifier: MIT
 *
 * @file: test_syslog.c
 * @brief emlog.h must compile alongside <syslog.h>, whose LOG_INFO macro
 *        collides with the deprecated log.h names.
 */

#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include "test_harness.h"

#include "emlog.h"
#include "emlog_version.h"

_Static_assert(EMLOG_VERSION_MAJOR >= 1, "version header is usable");

static uint32_t
fake_timestamp(void)
{
        return 7u;
}

TEST_CASE(test_emlog_with_syslog)
{
        struct emlog_ctx ctx;
        emlog_init(&ctx, fake_timestamp);

        emlog_event(&ctx, EMLOG_WARN, "syslog LOG_INFO=%d", LOG_INFO);

        const struct emlog_entry *e0 = emlog_get_entry(&ctx, 0u);
        TEST_ASSERT(e0 != NULL);
        TEST_ASSERT(e0->timestamp == 7u);
        TEST_ASSERT(e0->level == (uint16_t)EMLOG_WARN);
        TEST_ASSERT(strcmp(e0->msg, "syslog LOG_INFO=6") == 0);
}

int
main(void)
{
        run_test(test_emlog_with_syslog, "test_emlog_with_syslog");
        return EXIT_SUCCESS;
}
