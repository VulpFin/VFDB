// Copyright (C) 2025 TG11
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
// 
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once
/*
 * VFDB logging
 * Build flags:
 *   -D VFDB_DEBUG         -> default level DEBUG
 *   -D VFDB_LOG_LEVEL=N   -> override default level
 *   -D VFDB_LOG_DISABLE   -> strip logging
 */

#include <stdio.h>

#define ZSTR(s) ((s) ? (s) : "(null)")

#ifdef VFDB_LOG_DISABLE
#define VFDB_LOG_ENABLED 0
#else
#define VFDB_LOG_ENABLED 1
#endif

/* Levels (higher = chattier) */
typedef enum vfdb_log_level
{
    VFDB_LL_FATAL = 0,
    VFDB_LL_ERROR = 1,
    VFDB_LL_WARN = 2,
    VFDB_LL_INFO = 3,
    VFDB_LL_DEBUG = 4,
    VFDB_LL_TRACE = 5
} vfdb_log_level;

/* Compile-time default */
#ifndef VFDB_LOG_LEVEL
#ifdef VFDB_DEBUG
#define VFDB_LOG_LEVEL VFDB_LL_DEBUG
#else
#define VFDB_LOG_LEVEL VFDB_LL_WARN
#endif
#endif

typedef enum
{
    VFDB_COL_DEFAULT = -1, // use color derived from level
    VFDB_COL_RED = 1,
    VFDB_COL_YELLOW,
    VFDB_COL_GREEN,
    VFDB_COL_CYAN,
    VFDB_COL_BLUE,
    VFDB_COL_MAGENTA,
    VFDB_COL_WHITE,
    VFDB_COL_GRAY
} vfdb_log_color;

#if VFDB_LOG_ENABLED

void vfdb_log_set_level(vfdb_log_level lvl);
vfdb_log_level vfdb_log_get_level(void);
void vfdb_log_set_file(FILE *fp);  /* default: stderr */
void vfdb_log_write(vfdb_log_level lvl, const char *file, int line,
                    const char *func, const char *fmt, ...);
void vfdb_log_init_from_env(void); /* VFDB_LOG, VFDB_LOG_FILE */
void vfdb_log_init_once(void);

/* printf-style backend used by macros */
void vfdb_log_printf_(vfdb_log_level lvl,
                      const char *file, int line,
                      const char *func,
                      const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;

void vfdb_hexdump(const void *data, size_t len, const char *label);
void vfdb_dump_tail(const char *tag, const char *s, size_t max_chars);

/* Custom-log entry point (printf-style). "header" can be NULL.
 * If to_file != 0, writes to g_fp (or stderr if unset); else writes to stdout.
 * If color == VFDB_COL_DEFAULT, the level-based color is used.
 */
void vfdb_log_custom_(vfdb_log_level lvl, const char *header, int to_file,
                      vfdb_log_color color,
                      const char *file, int line, const char *func,
                      const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 8, 9)))
#endif
    ;

/* Convenience macros */
#define VFDB_LOG(L, fmt, ...)                                                          \
    do                                                                                 \
    {                                                                                  \
        if ((L) <= vfdb_log_get_level())                                               \
            vfdb_log_printf_((L), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__); \
    } while (0)

#define LOG_FATAL(fmt, ...) VFDB_LOG(VFDB_LL_FATAL, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) VFDB_LOG(VFDB_LL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) VFDB_LOG(VFDB_LL_WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) VFDB_LOG(VFDB_LL_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) VFDB_LOG(VFDB_LL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) VFDB_LOG(VFDB_LL_TRACE, fmt, ##__VA_ARGS__)
#define LOG_CUST(lvl, header, to_file, color, fmt, ...)   \
    vfdb_log_custom_((lvl), (header), (to_file), (color), \
                     __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__)

#define TRACE() LOG_TRACE("trace")

#ifdef VFDB_DEBUG
#include <stdlib.h>
#define VFDB_ASSERT(x)                             \
    do                                             \
    {                                              \
        if (!(x))                                  \
        {                                          \
            LOG_ERROR("assertion failed: %s", #x); \
            abort();                               \
        }                                          \
    } while (0)
#else
#define VFDB_ASSERT(x) ((void)0)
#endif

#else /* logging disabled */

#define vfdb_log_set_level(x) ((void)0)
#define vfdb_log_get_level() ((vfdb_log_level)0)
#define vfdb_log_set_file(x) ((void)0)
#define vfdb_log_init_from_env() ((void)0)
#define vfdb_log_init_once() ((void)0)
#define vfdb_log_printf_(...) ((void)0)
#define vfdb_hexdump(...) ((void)0)
#define vfdb_dump_tail(...) ((void)0)
#define LOG_FATAL(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
#define LOG_WARN(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#define LOG_TRACE(...) ((void)0)
#define TRACE() ((void)0)
#define VFDB_ASSERT(x) ((void)0)

#endif /* VFDB_LOG_ENABLED */
