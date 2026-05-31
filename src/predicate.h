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

#pragma once

#include <stdint.h>
#include "vfdb.h"      /* for vf_type */
#include "catalog.h"   /* for VFTable */

/* Comparison operators supported in WHERE predicates */
typedef enum {
    OP_EQ = 0,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE
} PredOp;

/* Simple predicate for WHERE col <op> constant (MVP) */
typedef struct {
    int has;           /* 0 = no predicate (always match) */
    int col_idx;       /* column index in the table */
    vf_type value_type;
    int64_t i64;       /* used for INT, REAL (as bits), BOOL */
    char *txt;         /* used for TEXT/BLOB (owned by caller or stmt) */
    PredOp op;
} Predicate;

/* Parse an operator from the input stream */
int parse_op(const char **p, PredOp *op);

/* Parse an optional WHERE predicate.
 * Returns:
 *   1  = predicate successfully parsed
 *   0  = no WHERE clause found
 *  -1  = parse error (unknown column, bad operator, etc.)
 */
int parse_where_pred(const char **p, const VFTable *t, Predicate *out);

/* Evaluate whether a row matches the predicate.
 * Used by DELETE and UPDATE paths.
 */
int pred_match_row(const VFTable *t, Predicate *pred, int64_t *ints, char **txts);

/* Comparison helpers (exported so legacy code in vfdb.c can use them during transition) */
int int_cmp(int64_t a, int64_t b, PredOp op);
int real_cmp(double a, double b, PredOp op);
int str_cmp(const char *a, const char *b, PredOp op);