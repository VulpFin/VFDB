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

// src/heap.h
#pragma once
#include <stdio.h>
#include <stdint.h>
#include "catalog.h"

/* Row flags */
#define VF_ROW_TOMBSTONE 0x01

typedef struct
{
    FILE *fp;
} HeapScan;

/* Append one row (values as string/int). texts[i] may be NULL for INT cols */
int heap_append_row(const VFTable *t, const int64_t *ints, const char *const *texts);

/* Open a scan over table heap */
int heap_scan_open(const VFTable *t, HeapScan *sc);
void heap_scan_close(HeapScan *sc);
/* Read next row.
   Returns 1 if a row was read, 0 on EOF, <0 on error.
   Outputs:
     - ints_out/texts_out: the decoded values (mallocs TEXT cells; caller frees)
     - row_offset: byte offset of **row start** (flags byte)
     - row_flags:  flags byte read
*/
int heap_scan_next_ex(const VFTable *t, HeapScan *sc,
                      int64_t *ints_out, char **texts_out,
                      long *row_offset, unsigned char *row_flags);

/* Convenience wrapper that skips tombstoned rows (keeps prior behavior) */
static inline int heap_scan_next(const VFTable *t, HeapScan *sc,
                                 int64_t *ints_out, char **texts_out)
{
    long off;
    unsigned char flags;
    for (;;)
    {
        int rc = heap_scan_next_ex(t, sc, ints_out, texts_out, &off, &flags);
        if (rc <= 0)
            return rc;
        if (!(flags & VF_ROW_TOMBSTONE))
            return 1; /* live row */
        /* free text cells for tombstoned rows before skipping */
        for (int i = 0; i < t->ncols; i++)
            if (t->cols[i].type == VF_T_TEXT && texts_out[i])
            {
                free(texts_out[i]);
                texts_out[i] = NULL;
            }
    }
}

/* Mark the row at 'offset' as tombstone (in-place) */
int heap_mark_tombstone(const VFTable *t, long offset);
