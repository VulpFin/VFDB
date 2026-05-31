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
 */

/* src/predicate.c
 * Predicate parsing and evaluation for WHERE clauses (MVP)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "predicate.h"
#include "compat_layer.h"
#include "vfdb_log.h"
#include "vf_type.h"


/* Forward declarations for helper functions defined later in this file */
static void skip_ws(const char **p);
static int parse_ident(const char **p, char *out, int outsz);
static int64_t real_to_bits(double value);
static double bits_to_real(int64_t bits);
static int parse_int_literal(const char **p, int64_t *out);
static int parse_real_literal(const char **p, int64_t *out_bits);
static int parse_bool_literal(const char **p, int64_t *out);
static int is_hex_digit_char(char ch);
static int parse_quoted(const char **p, char *out, int outsz);
static int parse_blob_literal(const char **p, char **out_txt);
static int parse_value_for_type(const char **p, vf_type type, int64_t *out_i64, char **out_txt);

#define DUMP_TAIL(tag, p) vfdb_dump_tail((tag), (p), 60)

/* ============================================================
   Operator parsing
   ============================================================ */

int parse_op(const char **p, PredOp *op)
{
    vfdb_log_init_once();
    LOG_DEBUG("parse op at %p", (void*)p);

    const char *s = *p;

    if (s[0] == '!' && s[1] == '=') {
        *op = OP_NE;
        s += 2;
    } else if (s[0] == '<' && s[1] == '=') {
        *op = OP_LE;
        s += 2;
    } else if (s[0] == '>' && s[1] == '=') {
        *op = OP_GE;
        s += 2;
    } else if (s[0] == '=') {
        *op = OP_EQ;
        s += 1;
    } else if (s[0] == '<') {
        *op = OP_LT;
        s += 1;
    } else if (s[0] == '>') {
        *op = OP_GT;
        s += 1;
    } else {
        return 0;
    }

    *p = s;
    return 1;
}

/* ============================================================
   Comparison helpers
   ============================================================ */

int int_cmp(int64_t a, int64_t b, PredOp op)
{
    switch (op) {
        case OP_EQ: return a == b;
        case OP_NE: return a != b;
        case OP_LT: return a < b;
        case OP_LE: return a <= b;
        case OP_GT: return a > b;
        case OP_GE: return a >= b;
    }
    return 0;
}

int real_cmp(double a, double b, PredOp op)
{
    switch (op) {
        case OP_EQ: return a == b;
        case OP_NE: return a != b;
        case OP_LT: return a < b;
        case OP_LE: return a <= b;
        case OP_GT: return a > b;
        case OP_GE: return a >= b;
    }
    return 0;
}

int str_cmp(const char *a, const char *b, PredOp op)
{
    int eq = (strcmp(a ? a : "", b ? b : "") == 0);
    if (op == OP_EQ) return eq;
    if (op == OP_NE) return !eq;
    return 0; /* other operators not valid for TEXT in current MVP */
}

/* ============================================================
   Predicate evaluation
   ============================================================ */

int pred_match_row(const VFTable *t, Predicate *pred, int64_t *ints, char **txts)
{
    vfdb_log_init_once();
    LOG_DEBUG("pred match row pred %p", (void*)pred);

    if (!pred || !pred->has)
        return 1;

    int c = pred->col_idx;

    if (vf_type_uses_real(pred->value_type)) {
        if (!vf_type_uses_real(t->cols[c].type))
            return 0;
        return real_cmp(bits_to_real(ints[c]), bits_to_real(pred->i64), pred->op);
    }

    if (vf_type_uses_int(pred->value_type)) {
        if (!vf_type_uses_int(t->cols[c].type))
            return 0;
        return int_cmp(ints[c], pred->i64, pred->op);
    } else {
        if (!vf_type_uses_text(t->cols[c].type))
            return 0;
        return str_cmp(txts[c] ? txts[c] : "", pred->txt ? pred->txt : "", pred->op);
    }
}

/* ============================================================
   WHERE predicate parser
   ============================================================ */

int parse_where_pred(const char **p, const VFTable *t, Predicate *out)
{
    vfdb_log_init_once();
    LOG_DEBUG("parse where pred table %p", (void*)t);

    Predicate pred = {0};
    const char *s = *p;
    skip_ws(&s);

    /* no WHERE => no predicate */
    if (strncasecmp(s, "WHERE", 5) != 0) {
        *out = pred;
        return 0;
    }

    s += 5;
    skip_ws(&s);
    DUMP_TAIL("WHERE tail@col", s);

    /* column name */
    char cname[64];
    if (!parse_ident(&s, cname, sizeof cname)) {
        DUMP_TAIL("WHERE parse_ident failed at", s);
        return -1;
    }

    int col = -1;
    for (int i = 0; i < t->ncols; i++) {
        if (t->cols[i].name && !vfdb_stricmp(t->cols[i].name, cname)) {
            col = i;
            break;
        }
    }
    if (col < 0) {
        LOG_DEBUG("WHERE unknown column '%s'", cname);
        return -1;
    }

    skip_ws(&s);
    DUMP_TAIL("WHERE tail@op", s);

    /* operator */
    PredOp op = OP_EQ;
    if (!parse_op(&s, &op)) {
        DUMP_TAIL("WHERE parse_op failed at", s);
        return -1;
    }

    /* Non-numeric values only allow = and != for now. */
    if (!vf_type_is_numeric(t->cols[col].type) && !(op == OP_EQ || op == OP_NE)) {
        LOG_DEBUG("WHERE invalid op for non-numeric column '%s'", cname);
        return -1;
    }

    skip_ws(&s);
    DUMP_TAIL("WHERE tail@const", s);

    pred.has = 1;
    pred.col_idx = col;
    pred.op = op;
    pred.value_type = t->cols[col].type;

    /* constant value */
    if (!parse_value_for_type(&s, pred.value_type, &pred.i64, &pred.txt)) {
        DUMP_TAIL("WHERE const parse failed at", s);
        return -1;
    }

    *p = s;
    *out = pred;
    DUMP_TAIL("WHERE done@", s);
    return 1;
}

/* ============================================================
   Helper functions temporarily duplicated here for predicate module.
   These will be properly moved/shared in later phases.
   ============================================================ */

static void skip_ws(const char **p)
{
    vfdb_log_init_once();
    LOG_DEBUG("skip whitespace at %p", (void*)p);
    while (**p && isspace((unsigned char)**p))
        (*p)++;
}

static int parse_ident(const char **p, char *out, int outsz)
{
    vfdb_log_init_once();
    LOG_DEBUG("parse ident at %p", (void*)p);
    while (**p && isspace((unsigned char)**p))
        (*p)++;
    int i = 0;
    while (**p && (isalnum((unsigned char)**p) || **p == '_'))
    {
        if (i + 1 < outsz)
            out[i++] = **p;
        (*p)++;
    }
    out[i] = 0;
    return i > 0;
}

static int64_t real_to_bits(double value)
{
    int64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static double bits_to_real(int64_t bits)
{
    double value = 0.0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static int parse_int_literal(const char **p, int64_t *out)
{
    const char *s = *p;
    int sign = 1;
    if (*s == '+' || *s == '-')
    {
        if (*s == '-')
            sign = -1;
        s++;
    }
    if (!isdigit((unsigned char)*s))
        return 0;
    long long v = 0;
    while (isdigit((unsigned char)*s))
    {
        v = v * 10 + (*s - '0');
        s++;
    }
    *out = sign * v;
    *p = s;
    return 1;
}

static int parse_real_literal(const char **p, int64_t *out_bits)
{
    char *end = NULL;
    double v = strtod(*p, &end);
    if (end == *p)
        return 0;
    *out_bits = real_to_bits(v);
    *p = end;
    return 1;
}

static int parse_bool_literal(const char **p, int64_t *out)
{
    const char *s = *p;
    if (!strncasecmp(s, "TRUE", 4) && !isalnum((unsigned char)s[4]) && s[4] != '_')
    {
        *out = 1;
        *p = s + 4;
        return 1;
    }
    if (!strncasecmp(s, "FALSE", 5) && !isalnum((unsigned char)s[5]) && s[5] != '_')
    {
        *out = 0;
        *p = s + 5;
        return 1;
    }
    if (*s == '1' || *s == '0')
    {
        *out = (*s == '1') ? 1 : 0;
        *p = s + 1;
        return 1;
    }
    return 0;
}

static int is_hex_digit_char(char ch)
{
    return isdigit((unsigned char)ch) ||
           (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

static int parse_quoted(const char **p, char *out, int outsz)
{
    vfdb_log_init_once();
    LOG_DEBUG("parse quoted at %p", (void*)p);
    const char *s = *p;
    if (*s != '\'')
        return 0;
    s++;
    int i = 0;
    while (*s && i + 1 < outsz)
    {
        if (*s == '\'')
        {
            if (s[1] == '\'')
            {
                out[i++] = '\'';
                s += 2;
                continue;
            }
            break;
        }
        out[i++] = *s++;
    }
    if (*s != '\'')
        return 0;
    out[i] = 0;
    s++; /* skip closing quote */
    *p = s;
    return 1;
}

static int parse_blob_literal(const char **p, char **out_txt)
{
    const char *s = *p;
    if ((s[0] == 'X' || s[0] == 'x') && s[1] == '\'')
    {
        s += 2;
        const char *start = s;
        while (*s && *s != '\'')
        {
            if (!is_hex_digit_char(*s))
                return 0;
            s++;
        }
        if (*s != '\'' || ((s - start) % 2) != 0)
            return 0;
        size_t len = (size_t)(s - start);
        char *hex = (char *)malloc(len + 1);
        if (!hex)
            return 0;
        memcpy(hex, start, len);
        hex[len] = 0;
        *out_txt = hex;
        *p = s + 1;
        return 1;
    }

    char buf[1024];
    if (!parse_quoted(&s, buf, sizeof buf))
        return 0;

    size_t len = strlen(buf);
    char *hex = (char *)malloc(len * 2 + 1);
    if (!hex)
        return 0;
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char)buf[i];
        hex[i * 2] = digits[ch >> 4];
        hex[i * 2 + 1] = digits[ch & 0x0F];
    }
    hex[len * 2] = 0;
    *out_txt = hex;
    *p = s;
    return 1;
}

static int parse_value_for_type(const char **p, vf_type type, int64_t *out_i64, char **out_txt)
{
    skip_ws(p);
    if (vf_type_uses_int(type) && type != VF_T_BOOL)
        return parse_int_literal(p, out_i64);
    if (vf_type_uses_real(type))
        return parse_real_literal(p, out_i64);
    if (type == VF_T_BOOL)
        return parse_bool_literal(p, out_i64);
    if (vf_type_uses_blob(type))
        return parse_blob_literal(p, out_txt);

    char buf[1024];
    if (!parse_quoted(p, buf, sizeof buf))
        return 0;
    *out_txt = vfdb_strdup(buf);
    return *out_txt != NULL;
}