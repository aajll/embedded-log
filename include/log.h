/**
 * SPDX-License-Identifier: MIT
 *
 * @file: log.h
 *
 * @brief
 *    Deprecated: include @c emlog.h and use the @c emlog_ and @c EMLOG_
 *    names.
 *
 *    This header maps the original names onto the prefixed API so existing
 *    callers compile unchanged. @c LOG_INFO collides with @c <syslog.h>;
 *    @c emlog.h does not. This header will be removed in the next major
 *    release.
 */

#ifndef LOG_H
#define LOG_H

#include "log_conf.h"

#include "emlog.h"

#define LOG_INFO        EMLOG_INFO
#define LOG_WARN        EMLOG_WARN
#define LOG_FAULT       EMLOG_FAULT
#define log_level       emlog_level
#define log_entry       emlog_entry
#define log_ctx         emlog_ctx
#define log_init        emlog_init
#define log_event       emlog_event
#define log_get_count   emlog_get_count
#define log_get_entry   emlog_get_entry
#define log_get_buffer  emlog_get_buffer
#define LOG_PRINTF_LIKE EMLOG_PRINTF_LIKE
#define LOG_ONCE        EMLOG_ONCE

#endif /* LOG_H */
