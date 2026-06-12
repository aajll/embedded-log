/**
 * SPDX-License-Identifier: MIT
 *
 * @file: log_conf.h
 *
 * @brief
 *    Configuration macros for the embedded logging facility.
 *
 *    Override any macro before including @c log.h (e.g. via a compiler
 *    @c -D flag or a project-level config header) to change the defaults.
 */

#ifndef LOG_CONF_H
#define LOG_CONF_H

#ifndef LOG_MSG_LEN
/**
 * @brief Maximum formatted message length, including the NUL terminator.
 *
 * Default: 64 bytes.
 */
#define LOG_MSG_LEN (64u)
#endif

#ifndef LOG_ENTRIES
/**
 * @brief Number of entries retained in the ring buffer.
 *
 * Default: 64 entries.
 */
#define LOG_ENTRIES (64u)
#endif

#endif /* LOG_CONF_H */
