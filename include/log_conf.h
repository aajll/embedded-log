/**
 * SPDX-License-Identifier: MIT
 *
 * @file: log_conf.h
 *
 * @brief
 *    Deprecated: include @c emlog_conf.h and use the @c EMLOG_ names.
 *
 *    A @c LOG_MSG_LEN or @c LOG_ENTRIES override applies to the matching
 *    @c EMLOG_ setting, and both names are defined afterwards. This header
 *    will be removed in the next major release.
 */

#ifndef LOG_CONF_H
#define LOG_CONF_H

#if defined(LOG_MSG_LEN) && !defined(EMLOG_MSG_LEN)
#define EMLOG_MSG_LEN LOG_MSG_LEN
#endif

#if defined(LOG_ENTRIES) && !defined(EMLOG_ENTRIES)
#define EMLOG_ENTRIES LOG_ENTRIES
#endif

#include "emlog_conf.h"

#ifndef LOG_MSG_LEN
#define LOG_MSG_LEN EMLOG_MSG_LEN
#endif

#ifndef LOG_ENTRIES
#define LOG_ENTRIES EMLOG_ENTRIES
#endif

#endif /* LOG_CONF_H */
