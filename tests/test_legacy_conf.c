/*
 * SPDX-License-Identifier: MIT
 *
 * @file: test_legacy_conf.c
 * @brief The deprecated LOG_ configuration and version names still apply
 *        through the forwarding headers.
 */

#define LOG_ENTRIES (8u)
#define LOG_MSG_LEN (24u)

#include <stdlib.h>

#include "log.h"
#include "log_version.h"

_Static_assert(EMLOG_ENTRIES == 8u, "LOG_ENTRIES sets EMLOG_ENTRIES");
_Static_assert(EMLOG_MSG_LEN == 24u, "LOG_MSG_LEN sets EMLOG_MSG_LEN");
_Static_assert(sizeof(((struct log_ctx *)0)->buffer)
                   == (8u * sizeof(struct emlog_entry)),
               "struct log_ctx is struct emlog_ctx");
_Static_assert(LOG_FAULT == EMLOG_FAULT, "levels map onto EMLOG_");
_Static_assert(LOG_VERSION_MAJOR == EMLOG_VERSION_MAJOR,
               "version names map onto EMLOG_");

int
main(void)
{
        return EXIT_SUCCESS;
}
