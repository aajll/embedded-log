/**
 * SPDX-License-Identifier: MIT
 *
 * @file: emlog_conf.h
 *
 * @brief
 *    Configuration macros for the embedded logging facility.
 *
 *    Override any macro before including @c emlog.h (e.g. via a compiler
 *    @c -D flag or a project-level config header) to change the defaults.
 */

#ifndef EMLOG_CONF_H
#define EMLOG_CONF_H

#ifndef EMLOG_MSG_LEN
/**
 * @brief Maximum formatted message length, including the NUL terminator.
 *
 * Default: 64 characters.
 */
#define EMLOG_MSG_LEN (64u)
#endif

#ifndef EMLOG_ENTRIES
/**
 * @brief Number of entries retained in the ring buffer.
 *
 * Default: 64 entries.
 */
#define EMLOG_ENTRIES (64u)
#endif

#endif /* EMLOG_CONF_H */
