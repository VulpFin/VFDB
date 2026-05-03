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

// src/catalog.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "catalog.h"
#include "vfdb_log.h"
#include "compat_layer.h"

    /* --- helpers --- */

static void cat_init(VFCatalog *cat)
{
    vfdb_log_init_once();
    LOG_DEBUG("cat init %p:%zu", (void*)cat, sizeof *cat);
    memset(cat, 0, sizeof *cat);
}

static void free_table(VFTable *t)
{
    vfdb_log_init_once();
    LOG_DEBUG("free table %p", (void*)t);
    if (!t)
        return;
    free(t->name);
    for (int i = 0; i < t->ncols; i++)
    {
        free(t->cols[i].name);
        free(t->cols[i].def_txt);
    }
    free(t->cols);
    memset(t, 0, sizeof *t);
}

static void base_without_ext(const char *path, char *out, size_t outsz)
{
    vfdb_log_init_once();
    LOG_DEBUG("base without ext path %s ", path);
    LOG_DEBUG("base without ext out %s ", out);
    //LOG_DEBUG("base without ext outsz %s ", outsz);
    /* copy path into out, then strip trailing extension (last '.') */
    size_t n = strlen(path);
    if (n >= outsz)
        n = outsz - 1;
    memcpy(out, path, n);
    out[n] = 0;
    char *dot = NULL;
    for (char *p = out; *p; ++p)
        if (*p == '.')
            dot = p;
    if (dot)
        *dot = 0;
}

static void make_meta_path(const char *db_path, char *out, size_t outsz)
{
    vfdb_log_init_once();
    LOG_DEBUG("make meta path db_path %s ", db_path);
    LOG_DEBUG("make meta path out %s ", out);
    //LOG_DEBUG("make meta path outsz %s ", outsz);
    /* <db>.meta */
    size_t n = strlen(db_path);
    if (n + 5 >= outsz)
    { /* ".meta" + NUL */
        /* fallback: truncate */
        n = outsz - 6;
    }
    memcpy(out, db_path, n);
    out[n] = 0;
    strcat(out, ".meta");
}

static void make_heap_path(const char *db_path, const char *table, char *out, size_t outsz)
{
    vfdb_log_init_once();
    LOG_DEBUG("make heap path db_path %s ", db_path);
    LOG_DEBUG("make heap path table %s ", table);
    LOG_DEBUG("make heap path out %s ", out);
    //LOG_DEBUG("make heap path outsz %s ", outsz);
    /* <db_base>__<table>.vfheap */
    char base[512];
    base_without_ext(db_path, base, sizeof base);

    if (outsz == 0)
        return;

    const char *sep = "__";
    const char *ext = ".vfheap";
    size_t sep_len = 2;
    size_t ext_len = 7;
    size_t base_len = strlen(base);
    size_t table_len = table ? strlen(table) : 0;
    size_t pos = 0;

    if (outsz <= sep_len + ext_len + 1)
    {
        out[0] = 0;
        return;
    }

    size_t max_table_len = outsz - sep_len - ext_len - 1;
    if (table_len > max_table_len)
        table_len = max_table_len;

    size_t max_base_len = outsz - sep_len - table_len - ext_len - 1;
    if (base_len > max_base_len)
        base_len = max_base_len;

    memcpy(out + pos, base, base_len);
    pos += base_len;
    memcpy(out + pos, sep, sep_len);
    pos += sep_len;
    if (table_len)
    {
        memcpy(out + pos, table, table_len);
        pos += table_len;
    }
    memcpy(out + pos, ext, ext_len);
    pos += ext_len;
    out[pos] = 0;
}

/* --- public API --- */

void cat_free(VFCatalog *cat)
{
    vfdb_log_init_once();
    LOG_DEBUG("cat free %p", (void*)cat);
    if (!cat)
        return;
    for (int i = 0; i < cat->n_tables; i++)
        free_table(&cat->tables[i]);
    free(cat->tables);
    memset(cat, 0, sizeof *cat);
}

int cat_find_table(const VFCatalog *cat, const char *name)
{
    vfdb_log_init_once();
    LOG_DEBUG("cat find table cat %p", (void*)cat);
    LOG_DEBUG("cat find table name %s ", name);
    if (!cat || !name)
        return -1;
    for (int i = 0; i < cat->n_tables; i++)
    {
        if (cat->tables[i].name && !vfdb_stricmp(cat->tables[i].name, name))
            return i;
    }
    return -1;
}

int cat_add_table(const char *db_path, VFCatalog *cat,
                  const char *name, const VFCol *cols, int ncols)
{
    vfdb_log_init_once();
    LOG_DEBUG("cat add table db_path %s ", db_path);
    LOG_DEBUG("cat add table cat %p", (void*)cat);
    LOG_DEBUG("cat add table name %s ", name);
    LOG_DEBUG("cat add table cols %p", (void*)cols);
    if (!db_path || !cat || !name || !cols || ncols <= 0)
        return -1;
    if (cat_find_table(cat, name) >= 0)
        return -1; /* already exists */

    VFTable *nt = (VFTable *)realloc(cat->tables, (cat->n_tables + 1) * sizeof(VFTable));
    if (!nt)
        return -1;
    cat->tables = nt;

    VFTable *t = &cat->tables[cat->n_tables];
    memset(t, 0, sizeof *t);
    t->name = vfdb_strdup(name);
    t->ncols = ncols;
    t->cols = (VFCol *)calloc(ncols, sizeof(VFCol));
    if (!t->name || !t->cols)
    {
        free_table(t);
        return -1;
    }

    for (int i = 0; i < ncols; i++)
    {
        t->cols[i].name = vfdb_strdup(cols[i].name);
        t->cols[i].type = cols[i].type;
        t->cols[i].param1 = cols[i].param1;
        t->cols[i].param2 = cols[i].param2;
        t->cols[i].not_null = cols[i].not_null;
        if (!t->cols[i].name)
        {
            free_table(t);
            return -1;
        }
    }

    make_heap_path(db_path, name, t->heap_path, sizeof t->heap_path);

    /* ensure heap file exists */
    FILE *hf = fopen(t->heap_path, "ab");
    if (hf)
        fclose(hf);

    cat->n_tables++;
    return cat->n_tables - 1;
}

void cat_load(const char *db_path, VFCatalog *cat)
{
    vfdb_log_init_once();
    LOG_DEBUG("cal_load path %s", db_path);
    LOG_DEBUG("cal_load cat %p", (void*)cat);
    cat_init(cat);
    if (!db_path)
        return;

    char meta[512];
    make_meta_path(db_path, meta, sizeof meta);
    FILE *fp = fopen(meta, "rb");
    if (!fp)
        return; /* empty catalog is fine */

    int n = 0;
    if (fread(&n, sizeof n, 1, fp) != 1)
    {
        fclose(fp);
        return;
    }
    if (n < 0 || n > 100000)
    {
        fclose(fp);
        return;
    } /* sanity */

    cat->tables = (VFTable *)calloc(n, sizeof(VFTable));
    if (!cat->tables)
    {
        fclose(fp);
        return;
    }

    cat->n_tables = n;
    for (int i = 0; i < n; i++)
    {
        uint32_t name_len = 0;
        if (fread(&name_len, sizeof name_len, 1, fp) != 1)
        {
            fclose(fp);
            cat_free(cat);
            return;
        }
        char *name = (char *)malloc(name_len + 1);
        if (!name)
        {
            fclose(fp);
            cat_free(cat);
            return;
        }
        if (name_len && fread(name, 1, name_len, fp) != name_len)
        {
            free(name);
            fclose(fp);
            cat_free(cat);
            return;
        }
        name[name_len] = 0;

        int ncols = 0;
        if (fread(&ncols, sizeof ncols, 1, fp) != 1)
        {
            free(name);
            fclose(fp);
            cat_free(cat);
            return;
        }

        VFTable *t = &cat->tables[i];
        memset(t, 0, sizeof *t);
        t->name = name;
        t->ncols = ncols;
        t->cols = (VFCol *)calloc(ncols, sizeof(VFCol));
        if (!t->cols)
        {
            fclose(fp);
            cat_free(cat);
            return;
        }

        for (int c = 0; c < ncols; c++)
        {
            uint32_t clen = 0;
            if (fread(&clen, sizeof clen, 1, fp) != 1)
            {
                fclose(fp);
                cat_free(cat);
                return;
            }
            char *cname = (char *)malloc(clen + 1);
            if (!cname)
            {
                fclose(fp);
                cat_free(cat);
                return;
            }
            if (clen && fread(cname, 1, clen, fp) != clen)
            {
                free(cname);
                fclose(fp);
                cat_free(cat);
                return;
            }
            cname[clen] = 0;

            int type = 0, p1 = 0, p2 = 0, nn = 0;
            if (fread(&type, sizeof type, 1, fp) != 1 ||
                fread(&p1, sizeof p1, 1, fp) != 1 ||
                fread(&p2, sizeof p2, 1, fp) != 1 ||
                fread(&nn, sizeof nn, 1, fp) != 1)
            {
                free(cname);
                fclose(fp);
                cat_free(cat);
                return;
            }
            t->cols[c].name = cname;
            t->cols[c].type = (vf_type)type;
            t->cols[c].param1 = p1;
            t->cols[c].param2 = p2;
            t->cols[c].not_null = nn;
            int hasdef = 0, defisint = 0;
            if (fread(&hasdef, sizeof hasdef, 1, fp) != 1 ||
                fread(&defisint, sizeof defisint, 1, fp) != 1)
            {
                free(cname);
                fclose(fp);
                cat_free(cat);
                return;
            }
            t->cols[c].has_default = hasdef;
            t->cols[c].def_is_int = defisint;
            if (hasdef)
            {
                if (defisint)
                {
                    if (fread(&t->cols[c].def_i64, sizeof t->cols[c].def_i64, 1, fp) != 1)
                    {
                        fclose(fp);
                        cat_free(cat);
                        return;
                    }
                }
                else
                {
                    uint32_t dlen = 0;
                    if (fread(&dlen, sizeof dlen, 1, fp) != 1)
                    {
                        fclose(fp);
                        cat_free(cat);
                        return;
                    }
                    if (dlen)
                    {
                        t->cols[c].def_txt = (char *)malloc(dlen + 1);
                        if (!t->cols[c].def_txt)
                        {
                            fclose(fp);
                            cat_free(cat);
                            return;
                        }
                        if (fread(t->cols[c].def_txt, 1, dlen, fp) != dlen)
                        {
                            fclose(fp);
                            cat_free(cat);
                            return;
                        }
                        t->cols[c].def_txt[dlen] = 0;
                    }
                    else
                    {
                        t->cols[c].def_txt = vfdb_strdup("");
                    }
                }
            }
        }

        make_heap_path(db_path, t->name, t->heap_path, sizeof t->heap_path);
    }

    fclose(fp);
}

void cat_save(const char *db_path, const VFCatalog *cat)
{
    vfdb_log_init_once();
    LOG_DEBUG("base without ext db_path %s ", db_path);
    LOG_DEBUG("base without ext cat %p", (void*)cat);
    if (!db_path || !cat)
        return;

    char meta[512];
    make_meta_path(db_path, meta, sizeof meta);
    FILE *fp = fopen(meta, "wb");
    if (!fp)
        return;

    int n = cat->n_tables;
    fwrite(&n, sizeof n, 1, fp);
    for (int i = 0; i < n; i++)
    {
        const VFTable *t = &cat->tables[i];
        uint32_t name_len = (uint32_t)(t->name ? strlen(t->name) : 0);
        fwrite(&name_len, sizeof name_len, 1, fp);
        if (name_len)
            fwrite(t->name, 1, name_len, fp);

        fwrite(&t->ncols, sizeof t->ncols, 1, fp);
        for (int c = 0; c < t->ncols; c++)
        {
            const VFCol *col = &t->cols[c];
            uint32_t clen = (uint32_t)(col->name ? strlen(col->name) : 0);
            fwrite(&clen, sizeof clen, 1, fp);
            if (clen)
                fwrite(col->name, 1, clen, fp);
            int type = (int)col->type;
            fwrite(&type, sizeof type, 1, fp);
            fwrite(&col->param1, sizeof col->param1, 1, fp);
            fwrite(&col->param2, sizeof col->param2, 1, fp);
            fwrite(&col->not_null, sizeof col->not_null, 1, fp);
            int hasdef = col->has_default;
            fwrite(&hasdef, sizeof hasdef, 1, fp);

            int defisint = col->def_is_int;
            fwrite(&defisint, sizeof defisint, 1, fp);

            /* Only write a default payload if a default actually exists */
            if (hasdef)
            {
                if (defisint)
                {
                    fwrite(&col->def_i64, sizeof col->def_i64, 1, fp);
                }
                else
                {
                    uint32_t dlen = (uint32_t)(col->def_txt ? strlen(col->def_txt) : 0);
                    fwrite(&dlen, sizeof dlen, 1, fp);
                    if (dlen)
                        fwrite(col->def_txt, 1, dlen, fp);
                }
            }
        }
    }

    fclose(fp);
}
