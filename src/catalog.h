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

// src/catalog.h
#pragma once
#include <stdint.h>
#include "vfdb.h" // <- brings in vf_type, VF_T_INT, VF_T_TEXT, etc.
#include "compat_layer.h"


typedef struct
{
    char *name;
    vf_type type; // from vfdb.h
    int param1;   /* e.g., VARCHAR(n) n; DECIMAL p (unused in MVP) */
    int param2;   /* e.g., DECIMAL s (unused in MVP) */
    int not_null; /* 0/1 (unused in MVP) */
    int has_default;
    int def_is_int; /* 1=int, 0=text */
    int64_t def_i64; /* int default */
    char *def_txt;   /* text default (owned) */
} VFCol;

typedef struct
{
    char *name;
    int ncols;
    VFCol *cols;
    char heap_path[512];
} VFTable;

typedef struct
{
    int n_tables;
    VFTable *tables;
} VFCatalog;

void cat_load(const char *db_path, VFCatalog *cat);         /* reads <db>.meta */
void cat_save(const char *db_path, const VFCatalog *cat);   /* writes <db>.meta */
int cat_find_table(const VFCatalog *cat, const char *name); /* returns idx or -1 */
/* pass db_path so we can derive heap path internally */
int cat_add_table(const char *db_path, VFCatalog *cat,
                  const char *name, const VFCol *cols, int ncols);
void cat_free(VFCatalog *cat);
