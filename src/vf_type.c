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

#include "vf_type.h"
#include "compat_layer.h"

/* Central table of all names and aliases */
typedef struct {
    const char *name;
    vf_type type;
} vf_type_entry;

static const vf_type_entry vf_type_map[] = {
    /* Integers */
    {"INT",        VF_T_INT},
    {"INTEGER",    VF_T_INT},
    {"SMALLINT",   VF_T_INT},
    {"BIGINT",     VF_T_INT},
    {"TINYINT",    VF_T_INT},

    /* Text */
    {"TEXT",       VF_T_TEXT},
    {"STRING",     VF_T_TEXT},
    {"CHAR",       VF_T_TEXT},
    {"VARCHAR",    VF_T_TEXT},
    {"CLOB",       VF_T_TEXT},

    /* Real / float / decimal */
    {"REAL",       VF_T_REAL},
    {"FLOAT",      VF_T_REAL},
    {"DOUBLE",     VF_T_REAL},
    {"NUMERIC",    VF_T_REAL},
    {"DECIMAL",    VF_T_REAL},

    /* Boolean */
    {"BOOL",       VF_T_BOOL},
    {"BOOLEAN",    VF_T_BOOL},

    /* Binary */
    {"BLOB",       VF_T_BLOB},
    {"BYTES",      VF_T_BLOB},
    {"BINARY",     VF_T_BLOB},

    /* Date/Time */
    {"DATE",       VF_T_DATETIME},
    {"TIME",       VF_T_DATETIME},
    {"DATETIME",   VF_T_DATETIME},
    {"TIMESTAMP",  VF_T_DATETIME},

    {NULL, 0}
};

/* Default fallback: TEXT (to be forgiving) */
vf_type vf_type_from_str(const char *s)
{
    if (!s)
        return VF_T_TEXT;
    for (const vf_type_entry *e = vf_type_map; e->name; ++e)
        if (!vfdb_stricmp(s, e->name))
            return e->type;
    return VF_T_TEXT;
}

/* Canonical name for display / PRAGMA schema */
const char *vf_type_to_str(vf_type t)
{
    switch (t)
    {
    case VF_T_INT:      return "INT";
    case VF_T_TEXT:     return "TEXT";
    case VF_T_REAL:     return "REAL";
    case VF_T_BOOL:     return "BOOL";
    case VF_T_BLOB:     return "BLOB";
    case VF_T_DATETIME: return "DATETIME";
    default:            return "TEXT";
    }
}
