/**
 * Copyright (C) 2025 TG11
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 **/

// VFDB logging implementation
#include "vfdb_log.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "compat_layer.h"


#ifdef _WIN32
  #include <windows.h>
#endif

#if VFDB_LOG_ENABLED

/* Global state */
static vfdb_log_level g_level = (vfdb_log_level)VFDB_LOG_LEVEL;
static FILE *g_fp = NULL;
static int g_initialized = 0;

#ifdef _WIN32
static HANDLE g_console = NULL;
static WORD   g_default_attr = 0;
#endif

/* ---- helpers ---- */
static const char* level_str(vfdb_log_level L) {
    switch (L) {
        case VFDB_LL_FATAL: return "FATAL";
        case VFDB_LL_ERROR: return "ERROR";
        case VFDB_LL_WARN:  return "WARN";
        case VFDB_LL_INFO:  return "INFO";
        case VFDB_LL_DEBUG: return "DEBUG";
        default:            return "TRACE";
    }
}

static void set_color(vfdb_log_level lvl) {
#ifdef _WIN32
    if (!g_console) {
        g_console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(g_console, &info))
            g_default_attr = info.wAttributes;
    }
    WORD color = g_default_attr;
    switch (lvl) {
        case VFDB_LL_FATAL:
        case VFDB_LL_ERROR: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        case VFDB_LL_WARN:  color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case VFDB_LL_INFO:  color = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case VFDB_LL_DEBUG: color = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
        case VFDB_LL_TRACE: color = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
    }
    SetConsoleTextAttribute(g_console, color);
#else
    const char *codes[] = {
        "\033[35m", /* FATAL (magenta) */
        "\033[31m", /* ERROR red */
        "\033[33m", /* WARN  yellow */
        "\033[32m", /* INFO  green */
        "\033[36m", /* DEBUG cyan */
        "\033[34m"  /* TRACE blue */
    };
    /* index by lvl, clamped */
    int idx = (lvl < VFDB_LL_FATAL) ? VFDB_LL_FATAL : (lvl > VFDB_LL_TRACE ? VFDB_LL_TRACE : lvl);
    fputs(codes[idx], stderr);
#endif
}

static void reset_color(void) {
#ifdef _WIN32
    if (g_console) SetConsoleTextAttribute(g_console, g_default_attr);
#else
    fputs("\033[0m", stderr);
#endif
}

static void set_color_by_enum(vfdb_log_color col)
{
#ifdef _WIN32
    if (!g_console)
    {
        g_console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(g_console, &info))
            g_default_attr = info.wAttributes;
    }
    WORD w = g_default_attr;
    switch (col)
    {
    case VFDB_COL_RED:
        w = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    case VFDB_COL_YELLOW:
        w = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case VFDB_COL_GREEN:
        w = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case VFDB_COL_CYAN:
        w = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    case VFDB_COL_BLUE:
        w = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    case VFDB_COL_MAGENTA:
        w = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    case VFDB_COL_WHITE:
        w = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    case VFDB_COL_GRAY:
        w = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    default:
        w = g_default_attr;
        break;
    }
    SetConsoleTextAttribute(g_console, w);
#else
    const char *esc = NULL;
    switch (col)
    {
    case VFDB_COL_RED:
        esc = "\033[31m";
        break;
    case VFDB_COL_YELLOW:
        esc = "\033[33m";
        break;
    case VFDB_COL_GREEN:
        esc = "\033[32m";
        break;
    case VFDB_COL_CYAN:
        esc = "\033[36m";
        break;
    case VFDB_COL_BLUE:
        esc = "\033[34m";
        break;
    case VFDB_COL_MAGENTA:
        esc = "\033[35m";
        break;
    case VFDB_COL_WHITE:
        esc = "\033[97m";
        break;
    case VFDB_COL_GRAY:
        esc = "\033[90m";
        break;
    default:
        esc = "\033[0m";
        break;
    }
    fputs(esc, stdout);
#endif
}

/* ---- public API ---- */
void vfdb_log_set_level(vfdb_log_level lvl) {
    if (lvl < VFDB_LL_FATAL) lvl = VFDB_LL_FATAL;
    if (lvl > VFDB_LL_TRACE) lvl = VFDB_LL_TRACE;
    g_level = lvl;
}

vfdb_log_level vfdb_log_get_level(void) {
    return g_level;
}



void vfdb_log_set_file(FILE *fp) {
    g_fp = fp;
}

void vfdb_log_custom_(vfdb_log_level lvl, const char *header, int to_file,
                      vfdb_log_color color,
                      const char *file, int line, const char *func,
                      const char *fmt, ...)
{
    if (lvl < g_level)
        return;

    /* choose stream */
    FILE *fp = to_file ? (g_fp ? g_fp : stderr) : stdout;

    /* timestamp */
    time_t now = time(NULL);
    struct tm tm_;
#if defined(_MSC_VER)
    localtime_s(&tm_, &now);
#else
    localtime_r(&now, &tm_);
#endif
    char tbuf[32];
    strftime(tbuf, sizeof tbuf, "%H:%M:%S", &tm_);

    /* short file */
    const char *slash = strrchr(file, '\\');
    if (!slash)
        slash = strrchr(file, '/');
    const char *base = slash ? slash + 1 : file;

    /* choose color */
    if (fp == stdout)
    {
        if (color == VFDB_COL_DEFAULT)
        {
            set_color(lvl);
        }
        else
        {
            set_color_by_enum(color);
        }
    }

    /* header label */
    const char *h = (header && *header) ? header : "CUSTOM";

    /* print header */
    static const char *lvl_names[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
    const char *lvl_name = "LVL";
    if (lvl >= VFDB_LL_ERROR && lvl <= VFDB_LL_TRACE)
    {
        lvl_name = lvl_names[lvl];
    }
    fprintf(fp, "%s [%s %s:%d %s|%s] ",
            tbuf, h, base, line, func, lvl_name);

    /* body */
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);

    /* newline + reset color if stdout */
    fputc('\n', fp);
    if (fp == stdout)
        reset_color();

    fflush(fp);
}

void vfdb_log_init_from_env(void) {
    const char *lvl = getenv("VFDB_LOG");
    if (lvl && *lvl) {
        if      (!strcmp(lvl, "TRACE")) vfdb_log_set_level(VFDB_LL_TRACE);
        else if (!strcmp(lvl, "DEBUG")) vfdb_log_set_level(VFDB_LL_DEBUG);
        else if (!strcmp(lvl, "INFO"))  vfdb_log_set_level(VFDB_LL_INFO);
        else if (!strcmp(lvl, "WARN"))  vfdb_log_set_level(VFDB_LL_WARN);
        else if (!strcmp(lvl, "ERROR")) vfdb_log_set_level(VFDB_LL_ERROR);
        else if (!strcmp(lvl, "FATAL")) vfdb_log_set_level(VFDB_LL_FATAL);
        else {
            int v = atoi(lvl);
            if (v >= VFDB_LL_FATAL && v <= VFDB_LL_TRACE)
                vfdb_log_set_level((vfdb_log_level)v);
        }
    }
    const char *path = getenv("VFDB_LOG_FILE");
    if (path && *path) {
        FILE *fp = fopen(path, "ab");
        if (fp) vfdb_log_set_file(fp);
    }
}

void vfdb_log_init_once(void) {
    if (g_initialized) return;
    g_initialized = 1;
#ifdef _WIN32
    g_console = GetStdHandle(STD_ERROR_HANDLE);
    if (g_console) {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(g_console, &info))
            g_default_attr = info.wAttributes;
    }
#endif
}

/* printf-style backend used by macros */
void vfdb_log_printf_(vfdb_log_level lvl,
                      const char *file, int line,
                      const char *func,
                      const char *fmt, ...)
{
    /* Filter: only log if message level <= threshold
       (since higher number = chattier) */
    if (lvl > g_level) return;

    FILE *fp = g_fp ? g_fp : stderr;

    /* timestamp */
    char tbuf[32];
    time_t now = time(NULL);
    struct tm tm_;
    localtime_r(&now, &tm_);
    strftime(tbuf, sizeof tbuf, "%H:%M:%S", &tm_);

    /* short file */
    const char *slash = strrchr(file, '\\');
    if (!slash) slash = strrchr(file, '/');
    const char *base = slash ? slash + 1 : file;

    /* header */
    set_color(lvl);
    fprintf(fp, "%s [%s %s:%d %s] ", tbuf, level_str(lvl), base, line, func);
    reset_color();

    /* body */
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);

    fputc('\n', fp);
    fflush(fp);
}

void vfdb_hexdump(const void *data, size_t len, const char *label) {
    if (VFDB_LL_DEBUG > g_level) return;
    FILE *fp = g_fp ? g_fp : stderr;
    const unsigned char *p = (const unsigned char *)data;
    fprintf(fp, "%s: size=%zu\n", label ? label : "hexdump", len);
    for (size_t i = 0; i < len; i += 16) {
        fprintf(fp, "%08zx  ", i);
        size_t j = 0;
        for (; j < 16 && i + j < len; ++j) fprintf(fp, "%02x ", p[i + j]);
        for (; j < 16; ++j) fputs("   ", fp);
        fputs(" |", fp);
        for (j = 0; j < 16 && i + j < len; ++j) {
            unsigned char c = p[i + j];
            fputc((c >= 32 && c < 127) ? c : '.', fp);
        }
        fputs("|\n", fp);
    }
    fflush(fp);
}

void vfdb_dump_tail(const char *tag, const char *s, size_t max_chars) {
    if (VFDB_LL_TRACE > g_level) return;
    if (!s) s = "";
    FILE *fp = g_fp ? g_fp : stderr;
    fprintf(fp, "DBG %s: '", tag ? tag : "tail");
    for (size_t i = 0; s[i] && i < max_chars; ++i) {
        char c = s[i];
        if      (c == '\n') fputs("\\n", fp);
        else if (c == '\r') fputs("\\r", fp);
        else if (c == '\t') fputs("\\t", fp);
        else                fputc(c, fp);
    }
    fputs("'\n", fp);
    fflush(fp);
}

void vfdb_log_write(vfdb_log_level lvl, const char *file, int line,
                    const char *func, const char *fmt, ...)
{
    if (lvl < g_level)
        return;

    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
#if defined(_MSC_VER)
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
#else
    vsnprintf(buf, sizeof(buf), fmt, ap);
#endif
    va_end(ap);

    /* Reuse the existing printer */
    vfdb_log_printf_(lvl, file, line, func, "%s", buf);
}

#endif /* VFDB_LOG_ENABLED */
