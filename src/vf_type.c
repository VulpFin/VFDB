/**
 * Copyright (C) 2025 TG11
 * GNU AGPL v3-or-later
 */

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "vf_type.h"
#include "compat_layer.h"

typedef struct {
    const char *name;
    vf_type type;
} vf_type_entry;

static const vf_type_entry vf_type_map[] = {
    {"SMALLINT", VF_T_INT},
    {"INT2", VF_T_INT},
    {"INTEGER", VF_T_INT},
    {"INT", VF_T_INT},
    {"INT4", VF_T_INT},
    {"BIGINT", VF_T_INT},
    {"INT8", VF_T_INT},
    {"SMALLSERIAL", VF_T_INT},
    {"SERIAL2", VF_T_INT},
    {"SERIAL", VF_T_INT},
    {"SERIAL4", VF_T_INT},
    {"BIGSERIAL", VF_T_INT},
    {"SERIAL8", VF_T_INT},

    {"REAL", VF_T_REAL},
    {"FLOAT4", VF_T_REAL},
    {"DOUBLE PRECISION", VF_T_REAL},
    {"FLOAT8", VF_T_REAL},
    {"FLOAT", VF_T_REAL},
    {"NUMERIC", VF_T_REAL},
    {"DECIMAL", VF_T_REAL},
    {"MONEY", VF_T_REAL},

    {"BOOLEAN", VF_T_BOOL},
    {"BOOL", VF_T_BOOL},

    {"BYTEA", VF_T_BLOB},
    {"BLOB", VF_T_BLOB},
    {"BYTES", VF_T_BLOB},
    {"BINARY", VF_T_BLOB},

    {"TEXT", VF_T_TEXT},
    {"VARCHAR", VF_T_TEXT},
    {"CHARACTER VARYING", VF_T_TEXT},
    {"CHAR", VF_T_TEXT},
    {"CHARACTER", VF_T_TEXT},
    {"BPCHAR", VF_T_TEXT},
    {"NAME", VF_T_TEXT},
    {"XML", VF_T_TEXT},
    {"STRING", VF_T_TEXT},
    {"CLOB", VF_T_TEXT},

    {"DATE", VF_T_DATE},
    {"TIME WITH TIME ZONE", VF_T_TIME},
    {"TIME WITHOUT TIME ZONE", VF_T_TIME},
    {"TIME", VF_T_TIME},
    {"TIMESTAMP WITH TIME ZONE", VF_T_TIMESTAMP},
    {"TIMESTAMPTZ", VF_T_TIMESTAMP},
    {"TIMESTAMP WITHOUT TIME ZONE", VF_T_TIMESTAMP},
    {"TIMESTAMP", VF_T_TIMESTAMP},
    {"DATETIME", VF_T_DATETIME},
    {"INTERVAL", VF_T_INTERVAL},

    {"JSONB", VF_T_JSON},
    {"JSON", VF_T_JSON},
    {"UUID", VF_T_UUID},
    {"INET", VF_T_INET},
    {"CIDR", VF_T_INET},
    {"MACADDR8", VF_T_MACADDR},
    {"MACADDR", VF_T_MACADDR},

    {NULL, 0}
};

static void normalize_decl(const char *src, char *dst, size_t dstsz)
{
    size_t out = 0;
    int prev_space = 1;
    int paren_depth = 0;

    if (dstsz == 0)
        return;

    for (const unsigned char *p = (const unsigned char *)src; p && *p; p++)
    {
        unsigned char ch = *p;
        if (ch == '(')
        {
            paren_depth++;
            continue;
        }
        if (ch == ')')
        {
            if (paren_depth > 0)
                paren_depth--;
            continue;
        }
        if (paren_depth > 0)
            continue;

        if (isspace(ch))
        {
            if (!prev_space && out + 1 < dstsz)
            {
                dst[out++] = ' ';
                prev_space = 1;
            }
            continue;
        }

        if (out + 1 < dstsz)
            dst[out++] = (char)toupper(ch);
        prev_space = 0;
    }

    while (out > 0 && dst[out - 1] == ' ')
        out--;
    dst[out] = 0;
}

vf_type vf_type_from_str(const char *s)
{
    char name[128];
    normalize_decl(s, name, sizeof name);
    if (!name[0])
        return VF_T_TEXT;

    for (const vf_type_entry *e = vf_type_map; e->name; ++e)
        if (!vfdb_stricmp(name, e->name))
            return e->type;

    for (const vf_type_entry *e = vf_type_map; e->name; ++e)
    {
        size_t n = strlen(e->name);
        if (!vfdb_strnicmp(name, e->name, n) &&
            (name[n] == 0 || isspace((unsigned char)name[n])))
            return e->type;
    }

    return VF_T_TEXT;
}

const char *vf_type_to_str(vf_type t)
{
    switch (t)
    {
    case VF_T_INT:       return "INT";
    case VF_T_TEXT:      return "TEXT";
    case VF_T_REAL:      return "REAL";
    case VF_T_BOOL:      return "BOOL";
    case VF_T_BLOB:      return "BYTEA";
    case VF_T_DATETIME:  return "DATETIME";
    case VF_T_DATE:      return "DATE";
    case VF_T_TIME:      return "TIME";
    case VF_T_TIMESTAMP: return "TIMESTAMP";
    case VF_T_INTERVAL:  return "INTERVAL";
    case VF_T_JSON:      return "JSONB";
    case VF_T_UUID:      return "UUID";
    case VF_T_INET:      return "INET";
    case VF_T_MACADDR:   return "MACADDR";
    default:             return "TEXT";
    }
}

int vf_type_uses_int(vf_type t)
{
    return t == VF_T_INT || t == VF_T_BOOL;
}

int vf_type_uses_real(vf_type t)
{
    return t == VF_T_REAL;
}

int vf_type_uses_blob(vf_type t)
{
    return t == VF_T_BLOB;
}

int vf_type_uses_text(vf_type t)
{
    return t == VF_T_TEXT || t == VF_T_BLOB || t == VF_T_DATETIME ||
           t == VF_T_DATE || t == VF_T_TIME || t == VF_T_TIMESTAMP ||
           t == VF_T_INTERVAL || t == VF_T_JSON || t == VF_T_UUID ||
           t == VF_T_INET || t == VF_T_MACADDR;
}

int vf_type_is_numeric(vf_type t)
{
    return vf_type_uses_int(t) || vf_type_uses_real(t);
}
