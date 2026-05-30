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

// src/heap.c  (simple: [flags][ncols:u16][nullmap:u16][col..])
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"
#include "vfdb.h"
#include "vfdb_log.h"
#include "vf_type.h"

static void write_u16(FILE *fp, unsigned v)
{
    vfdb_log_init_once();
    LOG_DEBUG("write u16 fp %p", (void*)fp);
    //LOG_DEBUG("write u16 unsigned %p", (void*)v);
    unsigned char b[2] = {v & 255, (v >> 8) & 255};
    fwrite(b, 1, 2, fp);
}
static void write_u32(FILE *fp, unsigned v)
{
    vfdb_log_init_once();
    LOG_DEBUG("write u32 fp %p", (void*)fp);
    //LOG_DEBUG("write u32 unsigned %p", (void*)v);
    unsigned char b[4] = {v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >> 24) & 255};
    fwrite(b, 1, 4, fp);
}
static int read_u16(FILE *fp, unsigned *v)
{
    vfdb_log_init_once();
    LOG_DEBUG("read u16 fp %p", (void*)fp);
    //LOG_DEBUG("read u16 unsigned %p", (void*)v);
    unsigned char b[2];
    if (fread(b, 1, 2, fp) != 2)
        return 0;
    *v = b[0] | (b[1] << 8);
    return 1;
}
static int read_u32(FILE *fp, unsigned *v)
{
    vfdb_log_init_once();
    LOG_DEBUG("read u32 fp %p", (void*)fp);
    //LOG_DEBUG("read u32 unsigned %p", (void*)v);
    unsigned char b[4];
    if (fread(b, 1, 4, fp) != 4)
        return 0;
    *v = b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
    return 1;
}

int heap_append_row(const VFTable *t, const int64_t *ints, const char *const *texts)
{
    vfdb_log_init_once();
    LOG_DEBUG("heap append row table %p", (void*)t);
    LOG_DEBUG("heap append row ints %p", (void*)ints);
    LOG_DEBUG("heap append row texts %p", (void*)texts);
    FILE *fp = fopen(t->heap_path, "ab");
    if (!fp)
        return 0;
    unsigned char flags = 0;
    fwrite(&flags, 1, 1, fp); /* flags: live row */
    write_u16(fp, (unsigned)t->ncols);
    write_u16(fp, 0); /* nullmap MVP = 0 (no NULLs) */
    for (int i = 0; i < t->ncols; i++)
    {
        if (vf_type_uses_int(t->cols[i].type) || vf_type_uses_real(t->cols[i].type))
        {
            /* fixed 8 bytes int64, bool, or double bit pattern */
            unsigned lo = (unsigned)(ints[i] & 0xFFFFFFFFu);
            unsigned hi = (unsigned)((uint64_t)ints[i] >> 32);
            write_u32(fp, lo);
            write_u32(fp, hi);
        }
        else
        { /* TEXT */
            const char *s = texts[i] ? texts[i] : "";
            unsigned L = (unsigned)strlen(s);
            write_u32(fp, L);
            fwrite(s, 1, L, fp);
        }
    }
    fclose(fp);
    return 1;
}

int heap_scan_open(const VFTable *t, HeapScan *sc)
{
    vfdb_log_init_once();
    LOG_DEBUG("heap scan open table %p", (void*)t);
    LOG_DEBUG("heap scan open scan %p", (void*)sc);
    sc->fp = fopen(t->heap_path, "rb");
    return sc->fp != NULL;
}
int heap_scan_next_ex(const VFTable *t, HeapScan *sc,
                      int64_t *ints_out, char **texts_out,
                      long *row_offset, unsigned char *row_flags)
{
    vfdb_log_init_once();
    LOG_DEBUG("heap append next ex table %p", (void*)t);
    LOG_DEBUG("heap append next ex scan %p", (void*)sc);
    LOG_DEBUG("heap append next ex ints out %p", (void*)ints_out);
    LOG_DEBUG("heap append next ex texts out %p", (void*)texts_out);
    LOG_DEBUG("heap append next ex row offset %p", (void*)row_offset);
    LOG_DEBUG("heap append next ex row flags %p", (void*)row_flags);
    if (!sc->fp)
        return 0;
    /* Remember start of row for tombstone marking */
    long start = ftell(sc->fp);
    if (start < 0)
        return 0;

    /* flags */
    unsigned char flags = 0;
    if (fread(&flags, 1, 1, sc->fp) != 1)
        return 0; /* EOF */

    unsigned ncols = 0, nullmap = 0;
    if (!read_u16(sc->fp, &ncols))
        return -1;
    if (!read_u16(sc->fp, &nullmap))
        return -1;

    for (unsigned i = 0; i < ncols; i++)
    {
        if (vf_type_uses_int(t->cols[i].type) || vf_type_uses_real(t->cols[i].type))
        {
            unsigned lo = 0, hi = 0;
            if (!read_u32(sc->fp, &lo) || !read_u32(sc->fp, &hi))
                return -1;
            ints_out[i] = ((int64_t)((uint64_t)hi << 32)) | lo;
            texts_out[i] = NULL;
        }
        else
        { /* TEXT */
            unsigned L = 0;
            if (!read_u32(sc->fp, &L))
                return -1;
            texts_out[i] = (char *)malloc(L + 1);
            if (L && fread(texts_out[i], 1, L, sc->fp) != L)
            {
                free(texts_out[i]);
                return -1;
            }
            texts_out[i][L] = '\0';
            ints_out[i] = 0;
        }
    }
    if (row_offset)
        *row_offset = start;
    if (row_flags)
        *row_flags = flags;
    return 1;
}

void heap_scan_close(HeapScan *sc)
{
    vfdb_log_init_once();
    LOG_DEBUG("heap scan close scan %p", (void*)sc);
    if (sc->fp)
        fclose(sc->fp);
    sc->fp = NULL;
}

int heap_mark_tombstone(const VFTable *t, long offset)
{
    vfdb_log_init_once();
    LOG_DEBUG("heap mark tombstone table %p", (void*)t);
    //LOG_DEBUG("heap mark tombstone offset %p", (void*)offset);
    FILE *fp = fopen(t->heap_path, "rb+");
    if (!fp)
        return 0;
    if (fseek(fp, offset, SEEK_SET) != 0)
    {
        fclose(fp);
        return 0;
    }
    unsigned char flags = 0;
    if (fread(&flags, 1, 1, fp) != 1)
    {
        fclose(fp);
        return 0;
    }
    flags |= VF_ROW_TOMBSTONE;
    if (fseek(fp, offset, SEEK_SET) != 0)
    {
        fclose(fp);
        return 0;
    }
    if (fwrite(&flags, 1, 1, fp) != 1)
    {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}
