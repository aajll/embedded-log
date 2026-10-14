/**
 * SPDX-License-Identifier: MIT
 *
 * @file: log_version.h
 *
 * @brief
 *    Deprecated: include @c emlog_version.h and use the @c EMLOG_VERSION
 *    names. This header will be removed in the next major release.
 */

#ifndef LOG_VERSION_H
#define LOG_VERSION_H

#include "emlog_version.h"

#define LOG_VERSION       EMLOG_VERSION
#define LOG_VERSION_MAJOR EMLOG_VERSION_MAJOR
#define LOG_VERSION_MINOR EMLOG_VERSION_MINOR
#define LOG_VERSION_PATCH EMLOG_VERSION_PATCH

#endif /* LOG_VERSION_H */
