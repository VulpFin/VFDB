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

/* src/vfdb.c */
/**
 * VFDB core (MVP+): CREATE TABLE, INSERT, SELECT * [WHERE col = const],
 * PRAGMA tables/schema, SELECT <int>, UPDATE/DELETE, and transaction replay.
 * INT and TEXT only for persisted values for now. Heap-append storage with
 * tombstones for UPDATE/DELETE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "vfdb.h"
#include "catalog.h"
#include "heap.h"
#include "vfdb_log.h"
#include "compat_layer.h"
#include "vf_type.h"
#define DUMP_TAIL(tag, p) vfdb_dump_tail((tag), (p), 60)

/* ----- Engine handles ----- */

/* ---- Transaction buffer (replay-on-commit) ---- */
typedef enum
{
    TX_NONE = 0,
    TX_ACTIVE = 1
} TxState;

typedef enum
{
    OP_EQ = 0,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE
} PredOp;

typedef struct
{
    char **sqls; /* recorded DML statements */
    int count;
    int cap;
} TxBuf;

static void txbuf_init(TxBuf *b) {
    vfdb_log_init_once();
    LOG_DEBUG("txbuf init buffer %p", (void*)b);
    memset(b, 0, sizeof *b);
}
static void txbuf_clear(TxBuf *b)
{
    vfdb_log_init_once();
    LOG_DEBUG("txbuf clear buffer %p", (void*)b);
    for (int i = 0; i < b->count; i++)
        free(b->sqls[i]);
    free(b->sqls);
    memset(b, 0, sizeof *b);
}
static void txbuf_push(TxBuf *b, const char *sql)
{
    vfdb_log_init_once();
    LOG_DEBUG("txbuf push buffer %p", (void*)b);
    LOG_DEBUG("txbuf push sql %s ", sql);
    if (b->count == b->cap)
    {
        b->cap = b->cap ? b->cap * 2 : 8;
        b->sqls = (char **)realloc(b->sqls, b->cap * sizeof(char *));
    }
    b->sqls[b->count++] = vfdb_strdup(sql);
}

struct VFDB
{
    FILE *fp;
    char *path;
    VFCatalog cat;
    TxState tx_state;
    TxBuf txbuf;
};

typedef enum
{
    STMT_NONE = 0,
    STMT_SELECT_CONST_INT,
    STMT_SELECT_ALL,
    STMT_CREATE_TABLE,
    STMT_INSERT_VALUES,
    STMT_PRAGMA_TABLES,
    STMT_PRAGMA_SCHEMA
} StmtKind;

/* simple predicate: WHERE col = const (INT or TEXT) */
typedef struct
{
    int has;
    int col_idx; /* which column */
    int is_int;  /* 1 if INT, 0 if TEXT */
    int64_t i64; /* int value */
    char *txt;   /* text value (owned by stmt) */
    PredOp op;
} Predicate;

struct VFDBStmt
{
    StmtKind kind;

    /* SELECT <int> */
    int emitted;
    int64_t const_int;

    /* SELECT * FROM table [WHERE ...] [LIMIT n] */
    int table_idx;
    int limit;
    int produced;
    int ncols;
    const VFTable *table_ptr;
    HeapScan scan;
    int64_t *row_ints; /* sized to ncols */
    char **row_texts;  /* sized to ncols */
    Predicate pred;

    /* PRAGMA tables/schema */
    int idx;   /* row index for PRAGMA iteration */
    int count; /* total rows for PRAGMA */
    const VFCatalog *cat_ptr;
    /* For PRAGMA TABLES we duplicate names into row_texts[0] per step. For SCHEMA we build from table_ptr. */
};

/* ---- forward decls needed for MSVC & order safety ---- */
static void skip_ws(const char **p);
static void skip_opt_semicolon(const char **p);
static int parse_ident(const char **p, char *out, int outsz);
static int parse_op(const char **p, PredOp *op);
static int parse_quoted(const char **p, char *out, int outsz);
static int parse_where_pred(const char **p, const VFTable *t, Predicate *out);

/* used in apply_delete_where_eq() and apply_update_set_eq_where_eq() */
static int pred_match_row(const VFTable *t, Predicate *pred, int64_t *ints, char **txts);

/* ============================================================
   Small parsing helpers
   ============================================================ */

static void skip_ws(const char **p)
{
    vfdb_log_init_once();
    LOG_DEBUG("skip whitespace at %p", (void*)p);
    while (**p && isspace((unsigned char)**p))
        (*p)++;
}

/* Call this at the very end of each statement parse, before returning OK */
static void skip_opt_semicolon(const char **p)
{
    vfdb_log_init_once();
    LOG_DEBUG("skip opt at ; %p", (void*)p);
    skip_ws(p);
    if (**p == ';')
    {
        (*p)++;
        skip_ws(p);
    }
}

static int parse_op(const char **p, PredOp *op)
{
    vfdb_log_init_once();
    LOG_DEBUG("parse op at %p", (void*)p);
    LOG_DEBUG("parse op op %p", (void*)op);
    const char *s = *p;
    if (s[0] == '!' && s[1] == '=')
    {
        *op = OP_NE;
        s += 2;
    }
    else if (s[0] == '<' && s[1] == '=')
    {
        *op = OP_LE;
        s += 2;
    }
    else if (s[0] == '>' && s[1] == '=')
    {
        *op = OP_GE;
        s += 2;
    }
    else if (s[0] == '=')
    {
        *op = OP_EQ;
        s += 1;
    }
    else if (s[0] == '<')
    {
        *op = OP_LT;
        s += 1;
    }
    else if (s[0] == '>')
    {
        *op = OP_GT;
        s += 1;
    }
    else
    {
        return 0;
    }
    *p = s;
    return 1;
}

static int parse_ident(const char **p, char *out, int outsz)
{
    vfdb_log_init_once();
    LOG_DEBUG("parse ident at %p", (void*)p);
    LOG_DEBUG("parse ident out %s ", out);
    //LOG_DEBUG("parse ident outsz %s ", outsz);
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

/* parse a quoted 'text' literal; doubled single quotes become one quote */
static int parse_quoted(const char **p, char *out, int outsz)
{
    vfdb_log_init_once();
    LOG_DEBUG("parse quoted at %p", (void*)p);
    LOG_DEBUG("parse quoted out %s ", out);
    //LOG_DEBUG("parse quoted outsz %s ", outsz);
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

/* SELECT <int> parser */
static int try_parse_select_const_int(const char *sql, int64_t *out)
{
    vfdb_log_init_once();
    LOG_DEBUG("try parse select const int sql %s ", sql);
    LOG_DEBUG("try parse select const int out %p ", (void*)out);
    const char *p = sql;
    while (*p && isspace((unsigned char)*p))
        p++;
    const char *kw = "select";
    for (; *kw; ++kw, ++p)
    {
        if (tolower((unsigned char)*p) != *kw)
            return 0;
    }
    while (*p && isspace((unsigned char)*p))
        p++;
    int sign = 1;
    if (*p == '+' || *p == '-')
    {
        if (*p == '-')
            sign = -1;
        p++;
    }
    if (!isdigit((unsigned char)*p))
        return 0;
    long long v = 0;
    while (isdigit((unsigned char)*p))
    {
        v = v * 10 + (*p - '0');
        p++;
    }
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p == ';')
        p++;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '\0')
        return 0;
    *out = sign * v;
    return 1;
}

/* ============================================================
   Public API
   ============================================================ */

vfdb_rc vfdb_open(const char *path, VFDB **out_db)
{
    vfdb_log_init_once();
    LOG_DEBUG("vfdb open path %s ", path);
    LOG_DEBUG("vfdb open out db %p", (void*)out_db);
    if (!path || !out_db)
        return VFDB_ERR;

    FILE *fp = fopen(path, "ab+"); /* ensure file exists */
    if (!fp)
        return VFDB_ERR;

    VFDB *db = (VFDB *)calloc(1, sizeof(VFDB));
    if (!db)
    {
        fclose(fp);
        return VFDB_ERR;
    }

    db->fp = fp;
    db->path = vfdb_strdup(path);     /* (use strdup on POSIX) */
    db->tx_state = TX_NONE;
    txbuf_init(&db->txbuf);
    cat_load(db->path, &db->cat); /* empty ok */

    *out_db = db;
    return VFDB_OK;
}

void vfdb_close(VFDB *db)
{
    vfdb_log_init_once();
    LOG_DEBUG("close db %p", (void*)db);
    if (!db)
        return;
    cat_save(db->path, &db->cat);
    cat_free(&db->cat);
    if (db->fp)
        fclose(db->fp);
    txbuf_clear(&db->txbuf);
    free(db->path);
    free(db);
}

/* Apply INSERT now */
static int apply_insert(VFDB *db, int tidx, int64_t *ints, char **texts)
{
    vfdb_log_init_once();
    LOG_DEBUG("apply insert db %p", (void*)db);
    //LOG_DEBUG("apply insert tidx %p", (void*)tidx);
    LOG_DEBUG("apply insert ints %p", (void*)ints);
    LOG_DEBUG("apply insert texts %p", (void*)texts);
    VFTable *t = &db->cat.tables[tidx];
    int ok = heap_append_row(t, ints, (const char *const *)texts);
    return ok ? 1 : 0;
}

/* Apply DELETE now: tombstone rows matching pred */
static int apply_delete_where_eq(VFDB *db, int tidx, Predicate *pred)
{
    vfdb_log_init_once();
    LOG_DEBUG("apply delete where eq db %p", (void*)db);
    //LOG_DEBUG("apply delete where eq tidx %p", (void*)tidx);
    LOG_DEBUG("apply delete where eq pred %p", (void*)pred);
    VFTable *t = &db->cat.tables[tidx];
    HeapScan sc;
    if (!heap_scan_open(t, &sc))
        return 0;
    int64_t *ints = (int64_t *)calloc(t->ncols, sizeof(int64_t));
    char **txts = (char **)calloc(t->ncols, sizeof(char *));
    long off;
    unsigned char flags;
    int changed = 0;
    while (1)
    {
        int rc = heap_scan_next_ex(t, &sc, ints, txts, &off, &flags);
        if (rc <= 0)
            break;
        /* skip already tombstoned */
        if (flags & VF_ROW_TOMBSTONE)
        {
            for (int i = 0; i < t->ncols; i++)
                if (t->cols[i].type == VF_T_TEXT)
                {
                    free(txts[i]);
                    txts[i] = NULL;
                }
            continue;
        }
        int match = 0;
        if (pred->is_int) 
            match = pred_match_row(t, pred, ints, txts);
        else
            match = pred_match_row(t, pred, ints, txts);

        if (match)
        {
            heap_mark_tombstone(t, off);
            changed++;
        }
        for (int i = 0; i < t->ncols; i++)
            if (t->cols[i].type == VF_T_TEXT)
            {
                free(txts[i]);
                txts[i] = NULL;
            }
    }
    heap_scan_close(&sc);
    free(ints);
    free(txts);
    return changed;
}

/* Apply UPDATE now: tombstone + append updated row with SET col=const (single col MVP) */
typedef struct
{
    int col_idx;
    int is_int;
    int64_t i64;
    char *txt;
} SetOne;

static int apply_update_set_eq_where_eq(VFDB *db, int tidx, SetOne *set, Predicate *pred)
{
    vfdb_log_init_once();
    LOG_DEBUG("apply update set eq where eq db %p", (void*)db);
    //LOG_DEBUG("apply update set eq where eq tidx %p", (void*)tidx);
    LOG_DEBUG("apply update set eq where eq set %p", (void*)set);
    LOG_DEBUG("apply update set eq where eq pred %p", (void*)pred);
    VFTable *t = &db->cat.tables[tidx];

    typedef struct
    {
        long off;      /* original row offset */
        int64_t *ints; /* snapshot of row INTs */
        char **txts;   /* snapshot of row TEXTs (owned strings) */
    } Match;

    Match *hits = NULL;
    int nhits = 0, cap = 0;

    HeapScan sc;
    if (!heap_scan_open(t, &sc))
        return 0;

    int64_t *ints = (int64_t *)calloc(t->ncols, sizeof(int64_t));
    char **txts = (char **)calloc(t->ncols, sizeof(char *));
    long off;
    unsigned char flags;
    int changed = 0;

    /* 1) Scan & snapshot matches (no writes during scan) */
    for (;;)
    {
        int rc = heap_scan_next_ex(t, &sc, ints, txts, &off, &flags);
        if (rc <= 0)
            break;

        /* skip tombstoned rows */
        if (flags & VF_ROW_TOMBSTONE)
        {
            for (int i = 0; i < t->ncols; i++)
                if (t->cols[i].type == VF_T_TEXT && txts[i])
                {
                    free(txts[i]);
                    txts[i] = NULL;
                }
            continue;
        }

        int match = 1;
        if (pred && pred->has)
            match = pred_match_row(t, pred, ints, txts);

        if (!match)
        {
            for (int i = 0; i < t->ncols; i++)
                if (t->cols[i].type == VF_T_TEXT && txts[i])
                {
                    free(txts[i]);
                    txts[i] = NULL;
                }
            continue;
        }

        /* snapshot this row */
        if (nhits == cap)
        {
            cap = cap ? cap * 2 : 8;
            hits = (Match *)realloc(hits, cap * sizeof(Match));
        }
        hits[nhits].off = off;
        hits[nhits].ints = (int64_t *)calloc(t->ncols, sizeof(int64_t));
        hits[nhits].txts = (char **)calloc(t->ncols, sizeof(char *));
        for (int i = 0; i < t->ncols; i++)
        {
            if (t->cols[i].type == VF_T_INT)
            {
                hits[nhits].ints[i] = ints[i];
            }
            else
            {
                hits[nhits].txts[i] = txts[i] ? vfdb_strdup(txts[i]) : vfdb_strdup("");
            }
        }
        nhits++;

        /* release current scan row's TEXT buffers */
        for (int i = 0; i < t->ncols; i++)
            if (t->cols[i].type == VF_T_TEXT && txts[i])
            {
                free(txts[i]);
                txts[i] = NULL;
            }
    }

    heap_scan_close(&sc);
    free(ints);
    free(txts);

    /* 2) Apply changes after scan completes */
    for (int k = 0; k < nhits; k++)
    {
        /* Build updated row from snapshot */
        int64_t *u_ints = (int64_t *)calloc(t->ncols, sizeof(int64_t));
        char **u_txts = (char **)calloc(t->ncols, sizeof(char *));
        for (int i = 0; i < t->ncols; i++)
        {
            if (t->cols[i].type == VF_T_INT)
                u_ints[i] = hits[k].ints[i];
            else
                u_txts[i] = hits[k].txts[i] ? vfdb_strdup(hits[k].txts[i]) : vfdb_strdup("");
        }

        /* apply SET */
        if (set->is_int)
        {
            u_ints[set->col_idx] = set->i64;
        }
        else
        {
            if (u_txts[set->col_idx])
                free(u_txts[set->col_idx]);
            u_txts[set->col_idx] = vfdb_strdup(set->txt ? set->txt : "");
        }

        /* tombstone original + append updated */
        heap_mark_tombstone(t, hits[k].off);
        heap_append_row(t, u_ints, (const char *const *)u_txts);
        changed++;

        /* cleanup */
        for (int i = 0; i < t->ncols; i++)
            if (t->cols[i].type == VF_T_TEXT && u_txts[i])
                free(u_txts[i]);
        free(u_txts);
        free(u_ints);

        for (int i = 0; i < t->ncols; i++)
            if (t->cols[i].type == VF_T_TEXT && hits[k].txts[i])
                free(hits[k].txts[i]);
        free(hits[k].txts);
        free(hits[k].ints);
    }
    free(hits);

    return changed;
}

/* Returns 1 if it parsed a predicate, 0 if no WHERE clause found, <0 on error */
static int parse_where_pred(const char **p, const VFTable *t, Predicate *out)
{
    vfdb_log_init_once();
    LOG_DEBUG("parse where pred p %p", (void*)p);
    LOG_DEBUG("parse where pred table %p", (void*)t);
    LOG_DEBUG("parse where pred out %p", (void*)out);
    Predicate pred = (Predicate){0};
    const char *s = *p;
    skip_ws(&s);

    /* no WHERE => no predicate */
    if (strncasecmp(s, "WHERE", 5) != 0)
    {
        *out = pred;
        return 0;
    }

    s += 5;
    skip_ws(&s);
    DUMP_TAIL("WHERE tail@col", s);

    /* column */
    char cname[64];
    if (!parse_ident(&s, cname, sizeof cname))
    {
        DUMP_TAIL("WHERE parse_ident failed at", s);
        return -1;
    }

    int col = -1;
    for (int i = 0; i < t->ncols; i++)
    {
        if (t->cols[i].name && !vfdb_stricmp(t->cols[i].name, cname))
        {
            col = i;
            break;
        }
    }
    if (col < 0)
    {
        LOG_DEBUG("WHERE unknown column '%s'", cname);
        return -1;
    }

    skip_ws(&s);
    DUMP_TAIL("WHERE tail@op", s);

    /* operator */
    PredOp op = OP_EQ;
    if (!parse_op(&s, &op))
    {
        DUMP_TAIL("WHERE parse_op failed at", s);
        return -1;
    }

    /* TEXT columns only allow = and != */
    if (t->cols[col].type == VF_T_TEXT && !(op == OP_EQ || op == OP_NE))
    {
        LOG_DEBUG("WHERE invalid op for TEXT column '%s'", cname);
        return -1;
    }

    skip_ws(&s);
    DUMP_TAIL("WHERE tail@const", s);

    pred.has = 1;
    pred.col_idx = col;
    pred.op = op;

    /* constant */
    if (t->cols[col].type == VF_T_INT)
    {
        int sign = 1;
        if (*s == '+' || *s == '-')
        {
            if (*s == '-')
                sign = -1;
            s++;
        }
        if (!isdigit((unsigned char)*s))
        {
            DUMP_TAIL("WHERE int const missing digits at", s);
            return -1;
        }
        long long v = 0;
        while (isdigit((unsigned char)*s))
        {
            v = v * 10 + (*s - '0');
            s++;
        }
        pred.is_int = 1;
        pred.i64 = sign * v;
    }
    else
    {
        char buf[1024];
        if (!parse_quoted(&s, buf, sizeof buf))
        {
            DUMP_TAIL("WHERE text const parse failed at", s);
            return -1;
        }
        pred.is_int = 0;
        pred.txt = vfdb_strdup(buf);
    }

    *p = s;
    *out = pred;
    DUMP_TAIL("WHERE done@", s);
    return 1;
}

/* ============================================================
   Prepare / Step / Finalize
   ============================================================ */

vfdb_rc vfdb_prepare(VFDB *db, const char *vfql, VFDBStmt **out_stmt)
{
    vfdb_log_init_once();
    LOG_DEBUG("vfdb prepare db %p", (void*)db);
    LOG_DEBUG("vfdb prepare vfql %s ", vfql);
    LOG_DEBUG("vfdb prepare out statement %p", (void*)out_stmt);
    if (!db || !vfql || !out_stmt)
        return VFDB_ERR;

    /* Fast path: SELECT <int> */
    int64_t cval = 0;
    if (try_parse_select_const_int(vfql, &cval))
    {
        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        if (!st)
            return VFDB_ERR;
        st->kind = STMT_SELECT_CONST_INT;
        st->const_int = cval;
        *out_stmt = st;
        return VFDB_OK;
    }

    const char *p = vfql;
    while (*p && isspace((unsigned char)*p))
        p++;

    /* BEGIN / COMMIT / ROLLBACK */
    if (!strncasecmp(p, "BEGIN", 5))
    {
        p += 5;
        db->tx_state = TX_ACTIVE;
        txbuf_clear(&db->txbuf);
        /* return a no-row stmt */
        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        st->kind = STMT_CREATE_TABLE; /* any no-row kind works */
        *out_stmt = st;
        return VFDB_OK;
    }
    if (!strncasecmp(p, "COMMIT", 6))
    {
        p += 6;
        if (db->tx_state == TX_ACTIVE)
        {
            /* snapshot & switch to autocommit */
            int n = db->txbuf.count;
            char **sqls = db->txbuf.sqls;
            db->txbuf.sqls = NULL;
            db->txbuf.count = 0;
            db->txbuf.cap = 0;

            db->tx_state = TX_NONE;

            /* replay each statement once, in autocommit */
            for (int i = 0; i < n; i++)
            {
                VFDBStmt *inner = NULL;
                if (vfdb_prepare(db, sqls[i], &inner) == VFDB_OK)
                {
                    while (vfdb_step(inner) == 1)
                    { /* consume rows if any */
                    }
                    vfdb_finalize(inner);
                }
                free(sqls[i]);
            }
            free(sqls);
            skip_opt_semicolon(&p);
            DUMP_TAIL("after opt ';'", p);
            skip_ws(&p);
            if (*p != '\0')
                return VFDB_ERR;
        }
        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        st->kind = STMT_CREATE_TABLE; /* no rows */
        *out_stmt = st;
        return VFDB_OK;
    }
    if (!strncasecmp(p, "ROLLBACK", 8))
    {
        p += 8;
        if (db->tx_state == TX_ACTIVE)
        {
            txbuf_clear(&db->txbuf);
            db->tx_state = TX_NONE;
        }
        skip_opt_semicolon(&p);
        DUMP_TAIL("after opt ';'", p);
        skip_ws(&p);
        if (*p != '\0')
            return VFDB_ERR;
        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        st->kind = STMT_CREATE_TABLE;
        *out_stmt = st;
        return VFDB_OK;
    }
    /* PRAGMA tables; */
    if (!strncasecmp(p, "PRAGMA", 6))
    {
        p += 6;
        while (isspace((unsigned char)*p))
            p++;
        if (!strncasecmp(p, "tables", 6))
        {
            VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
            if (!st)
                return VFDB_ERR;
            st->kind = STMT_PRAGMA_TABLES;
            st->ncols = 1;
            st->idx = 0;
            st->cat_ptr = &db->cat; /* <-- add */
            st->count = db->cat.n_tables;
            st->row_texts = (char **)calloc(1, sizeof(char *));
            st->row_ints = NULL;
            *out_stmt = st;
            return VFDB_OK;
        }
        else if (!strncasecmp(p, "schema", 6))
        {
            p += 6;
            while (isspace((unsigned char)*p))
                p++;
            char tname[64];
            if (!parse_ident(&p, tname, sizeof tname))
                return VFDB_ERR;
            int tidx = cat_find_table(&db->cat, tname);
            if (tidx < 0)
                return VFDB_ERR;
            VFTable *t = &db->cat.tables[tidx];

            VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
            if (!st)
                return VFDB_ERR;
            st->kind = STMT_PRAGMA_SCHEMA;
            st->table_idx = tidx;
            st->table_ptr = t;
            st->ncols = 2; /* name, type */
            st->idx = 0;
            st->count = t->ncols;
            st->row_texts = (char **)calloc(2, sizeof(char *));
            st->row_ints = NULL;
            *out_stmt = st;
            return VFDB_OK;
        }
        return VFDB_ERR;
    }

    /* CREATE TABLE name (col type,...)  -- INT | TEXT only (MVP) */
    if (!strncasecmp(p, "CREATE TABLE", 12))
    {
        p += 12;
        while (isspace((unsigned char)*p))
            p++;
        char tname[64];
        if (!parse_ident(&p, tname, sizeof tname))
            return VFDB_ERR;
        while (isspace((unsigned char)*p))
            p++;
        if (*p != '(')
            return VFDB_ERR;
        p++;

        VFCol cols[64];
        int ncols = 0;
        for (;;)
        {
            while (isspace((unsigned char)*p))
                p++;
            char cname[64];
            if (!parse_ident(&p, cname, sizeof cname))
                return VFDB_ERR;
            while (isspace((unsigned char)*p))
                p++;
            if (!strncasecmp(p, "INT", 3))
            {
                cols[ncols].type = VF_T_INT;
                p += 3;
            }
            else if (!strncasecmp(p, "TEXT", 4))
            {
                cols[ncols].type = VF_T_TEXT;
                p += 4;
            }
            else
                return VFDB_ERR;
            cols[ncols].name = vfdb_strdup(cname);
            cols[ncols].param1 = cols[ncols].param2 = cols[ncols].not_null = 0;
            cols[ncols].has_default = 0;
            cols[ncols].def_is_int = 1;
            cols[ncols].def_i64 = 0;
            cols[ncols].def_txt = NULL;
            ncols++;

            while (isspace((unsigned char)*p))
                p++;
            if (*p == ',')
            {
                p++;
                continue;
            }
            if (*p == ')')
            {
                p++;
                break;
            }
            return VFDB_ERR;
        }

        (void)cat_add_table(db->path, &db->cat, tname, cols, ncols);
        (void)cat_save(db->path, &db->cat);

        skip_opt_semicolon(&p);
        DUMP_TAIL("after opt ';'", p);
        skip_ws(&p);
        if (*p != '\0')
            return VFDB_ERR;

        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        if (!st)
            return VFDB_ERR;
        st->kind = STMT_CREATE_TABLE; /* DDL: no rows */
        *out_stmt = st;
        return VFDB_OK;
    }

    /* INSERT INTO name VALUES (...)   (single row MVP) */
    if (!strncasecmp(p, "INSERT INTO", 11))
    {
        p += 11;
        while (isspace((unsigned char)*p))
            p++;
        char tname[64];
        if (!parse_ident(&p, tname, sizeof tname))
            return VFDB_ERR;
        int tidx = cat_find_table(&db->cat, tname);
        if (tidx < 0)
            return VFDB_ERR;

        while (isspace((unsigned char)*p))
            p++;
        if (strncasecmp(p, "VALUES", 6) != 0)
            return VFDB_ERR;
        p += 6;
        while (isspace((unsigned char)*p))
            p++;
        if (*p != '(')
            return VFDB_ERR;
        p++;

        VFTable *t = &db->cat.tables[tidx];
        int64_t *ints = (int64_t *)calloc(t->ncols, sizeof(int64_t));
        char **texts = (char **)calloc(t->ncols, sizeof(char *));
        if (!ints || !texts)
            return VFDB_ERR;

        for (int i = 0; i < t->ncols; i++)
        {
            while (isspace((unsigned char)*p))
                p++;
            if (t->cols[i].type == VF_T_INT)
            {
                int sign = 1;
                if (*p == '+' || *p == '-')
                {
                    if (*p == '-')
                        sign = -1;
                    p++;
                }
                long long v = 0;
                if (!isdigit((unsigned char)*p))
                    return VFDB_ERR;
                while (isdigit((unsigned char)*p))
                {
                    v = v * 10 + (*p - '0');
                    p++;
                }
                ints[i] = sign * v;
            }
            else
            {
                char buf[1024];
                if (!parse_quoted(&p, buf, sizeof buf))
                    return VFDB_ERR;
                texts[i] = vfdb_strdup(buf);
            }
            while (isspace((unsigned char)*p))
                p++;
            if (i + 1 < t->ncols)
            {
                if (*p != ',')
                    return VFDB_ERR;
                p++;
            }
        }
        if (*p != ')')
            return VFDB_ERR;
        p++;

        /* optional trailing ';' and trailing-junk check */
        skip_opt_semicolon(&p);
        skip_ws(&p);
        if (*p != '\0')
            return VFDB_ERR;

        if (db->tx_state == TX_ACTIVE)
        {
            /* Buffer full SQL to replay on COMMIT, discard parsed values now */
            txbuf_push(&db->txbuf, vfql);
            /* free allocated values since we didn’t write them */
            for (int i = 0; i < t->ncols; i++)
                free(texts[i]);
            free(texts);
            free(ints);
        }
        else
        {
            /* Autocommit: apply immediately */
            if (!apply_insert(db, tidx, ints, texts))
                return VFDB_ERR;
            for (int i = 0; i < t->ncols; i++)
                free(texts[i]);
            free(texts);
            free(ints);
        }

        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        if (!st)
            return VFDB_ERR;
        st->kind = STMT_INSERT_VALUES; /* DML: no rows */
        *out_stmt = st;
        return VFDB_OK;
    }
    /* DELETE FROM name [WHERE col = const] */
    if (!strncasecmp(p, "DELETE FROM", 11))
    {
        p += 11;
        while (isspace((unsigned char)*p))
            p++;
        char tname[64];
        if (!parse_ident(&p, tname, sizeof tname))
            return VFDB_ERR;
        int tidx = cat_find_table(&db->cat, tname);
        if (tidx < 0)
            return VFDB_ERR;

        Predicate pred = {0};
        {
            int rc = parse_where_pred(&p, &db->cat.tables[tidx], &pred);
            if (rc < 0)
                return VFDB_ERR;
        }
        skip_opt_semicolon(&p);
        DUMP_TAIL("after opt ';'", p);
        skip_ws(&p);
        if (*p != '\0')
            return VFDB_ERR; /* unexpected trailing junk */

        if (db->tx_state == TX_ACTIVE)
        {
            txbuf_push(&db->txbuf, vfql);
        }
        else
        {
            (void)apply_delete_where_eq(db, tidx, &pred);
        }
        if (pred.txt)
            free(pred.txt);

        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        st->kind = STMT_INSERT_VALUES;
        *out_stmt = st;
        return VFDB_OK;
    }
    /* UPDATE name SET col=const [WHERE col=const]  (single SET MVP) */
    
    if (!strncasecmp(p, "UPDATE", 6))
    {
        TRACE();
        p += 6;
        skip_ws(&p);
        DUMP_TAIL("after UPDATE", p);
        while (isspace((unsigned char)*p))
            p++;
        char tname[64];
        if (!parse_ident(&p, tname, sizeof tname))
            return VFDB_ERR;
        int tidx = cat_find_table(&db->cat, tname);
        if (tidx < 0)
            return VFDB_ERR;
        VFTable *t = &db->cat.tables[tidx];

        while (isspace((unsigned char)*p))
            p++;
        if (strncasecmp(p, "SET", 3) != 0)
            return VFDB_ERR;
        p += 3;
        DUMP_TAIL("before SET value", p);
        while (isspace((unsigned char)*p))
            p++;
        char sname[64];
        if (!parse_ident(&p, sname, sizeof sname))
            return VFDB_ERR;
        int scol = -1;
        for (int i = 0; i < t->ncols; i++)
            if (!vfdb_stricmp(t->cols[i].name, sname))
            {
                scol = i;
                break;
            }
        if (scol < 0)
        {
            LOG_DEBUG("UPDATE unknown SET column '%s' in table '%s'", sname, t->name);
            return VFDB_ERR;
        }
        if (scol < 0)
            return VFDB_ERR;
        while (isspace((unsigned char)*p))
            p++;
        if (*p != '=')
            return VFDB_ERR;
        p++;
        while (isspace((unsigned char)*p))
            p++;

        TRACE();

        SetOne set = {0};
        set.col_idx = scol;
        if (t->cols[scol].type == VF_T_INT)
        {
            int sign = 1;
            if (*p == '+' || *p == '-')
            {
                if (*p == '-')
                    sign = -1;
                p++;
            }
            long long v = 0;
            if (!isdigit((unsigned char)*p))
                return VFDB_ERR;
            while (isdigit((unsigned char)*p))
            {
                v = v * 10 + (*p - '0');
                p++;
            }
            set.is_int = 1;
            set.i64 = sign * v;
        }
        else
        {
            char buf[1024];
            if (!parse_quoted(&p, buf, sizeof buf))
                return VFDB_ERR;
            set.is_int = 0;
            set.txt = vfdb_strdup(buf);
        }

        TRACE();
        DUMP_TAIL("before WHERE", p);
        /* WHERE (optional) */
        Predicate pred = (Predicate){0};
        {
            int rc = parse_where_pred(&p, t, &pred);
            LOG_TRACE("UPDATE parse_where_pred rc=%d", rc);
            if (rc < 0)
            {
                TRACE();
                return VFDB_ERR; /* syntax/column/op error */
            }
            /* rc == 0 => no WHERE; pred.has stays 0 */
        }

        /* optional trailing ';' */
        skip_opt_semicolon(&p);
        DUMP_TAIL("after opt ';'", p);
        skip_ws(&p);
        if (*p != '\0')
            return VFDB_ERR; /* unexpected trailing junk */
        TRACE();

        /* execute (tx-buffered or immediate) */
        if (db->tx_state == TX_ACTIVE)
        {
            txbuf_push(&db->txbuf, vfql);
        }
        else
        {
            (void)apply_update_set_eq_where_eq(db, tidx, &set, pred.has ? &pred : NULL);
        }

        if (!set.is_int && set.txt)
            free(set.txt);
        if (pred.txt)
            free(pred.txt);

        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        st->kind = STMT_INSERT_VALUES;
        *out_stmt = st;
        return VFDB_OK;
    }

    /* SELECT * FROM name [WHERE col = const] [LIMIT n] */
    if (!strncasecmp(p, "SELECT", 6))
    {
        p += 6;
        while (isspace((unsigned char)*p))
            p++;
        if (*p != '*')
            return VFDB_ERR;
        p++;
        while (isspace((unsigned char)*p))
            p++;
        if (strncasecmp(p, "FROM", 4) != 0)
            return VFDB_ERR;
        p += 4;
        while (isspace((unsigned char)*p))
            p++;
        char tname[64];
        if (!parse_ident(&p, tname, sizeof tname))
            return VFDB_ERR;
        int tidx = cat_find_table(&db->cat, tname);
        if (tidx < 0)
            return VFDB_ERR;

        Predicate pred = {0};
        {
            int rc = parse_where_pred(&p, &db->cat.tables[tidx], &pred);
            if (rc < 0)
                return VFDB_ERR;
        }

        int limit = 0;
        while (isspace((unsigned char)*p))
            p++;
        if (!strncasecmp(p, "LIMIT", 5))
        {
            p += 5;
            while (isspace((unsigned char)*p))
                p++;
            while (isdigit((unsigned char)*p))
            {
                limit = limit * 10 + (*p - '0');
                p++;
            }
        }

        VFTable *t = &db->cat.tables[tidx];

        VFDBStmt *st = (VFDBStmt *)calloc(1, sizeof *st);
        if (!st)
            return VFDB_ERR;
        st->kind = STMT_SELECT_ALL;
        st->table_idx = tidx;
        st->table_ptr = t;
        st->ncols = t->ncols;
        st->limit = limit;
        st->row_ints = (int64_t *)calloc(t->ncols, sizeof(int64_t));
        st->row_texts = (char **)calloc(t->ncols, sizeof(char *));
        if (!st->row_ints || !st->row_texts)
        {
            free(st->row_ints);
            free(st->row_texts);
            free(st);
            return VFDB_ERR;
        }
        if (!heap_scan_open(t, &st->scan))
        {
            free(st->row_ints);
            free(st->row_texts);
            free(st);
            return VFDB_ERR;
        }
        st->pred = pred;

        *out_stmt = st;
        return VFDB_OK;
    }

    return VFDB_ERR; /* unsupported SQL in this MVP */
}

static int int_cmp(int64_t a, int64_t b, PredOp op)
{
    vfdb_log_init_once();
    //LOG_DEBUG("int cmp a %p", (void*)a);
    //LOG_DEBUG("int cmp b %p", (void*)b);
    //LOG_DEBUG("int cmp op %p", (void*)op);
    switch (op)
    {
    case OP_EQ:
        return a == b;
    case OP_NE:
        return a != b;
    case OP_LT:
        return a < b;
    case OP_LE:
        return a <= b;
    case OP_GT:
        return a > b;
    case OP_GE:
        return a >= b;
    }
    return 0;
}
static int str_cmp(const char *a, const char *b, PredOp op)
{
    vfdb_log_init_once();
    //LOG_DEBUG("str cmp a %p", (void*)a);
    //LOG_DEBUG("str cmp b %p", (void*)b);
    //LOG_DEBUG("str cmp op %p", (void*)op);
    int eq = (strcmp(a ? a : "", b ? b : "") == 0);
    if (op == OP_EQ)
        return eq;
    if (op == OP_NE)
        return !eq;
    return 0; /* others invalid for TEXT */
}

static int row_matches_pred(VFDBStmt *st)
{
    vfdb_log_init_once();
    LOG_DEBUG("row matches pred stmt %p", (void*)st);
    if (!st->pred.has)
        return 1;
    int c = st->pred.col_idx;
    if (st->pred.is_int)
    {
        if (st->table_ptr->cols[c].type != VF_T_INT)
            return 0;
        return int_cmp(st->row_ints[c], st->pred.i64, st->pred.op);
    }
    else
    {
        if (st->table_ptr->cols[c].type != VF_T_TEXT)
            return 0;
        return str_cmp(st->row_texts[c], st->pred.txt, st->pred.op);
    }
}

int vfdb_step(VFDBStmt *st)
{
    vfdb_log_init_once();
    LOG_DEBUG("vfdb step stmt %p", (void*)st);
    if (!st)
        return -1;

    switch (st->kind)
    {
    case STMT_SELECT_CONST_INT:
        if (!st->emitted)
        {
            st->emitted = 1;
            return 1;
        }
        return 0;

    case STMT_PRAGMA_TABLES:
    {
        if (st->idx >= st->count)
            return 0;
        if (st->row_texts[0])
        {
            free(st->row_texts[0]);
            st->row_texts[0] = NULL;
        }
        const VFTable *t = &st->cat_ptr->tables[st->idx];
        st->row_texts[0] = vfdb_strdup(t->name ? t->name : "");
        st->idx++;
        return 1;
    }

    case STMT_PRAGMA_SCHEMA:
    {
        if (st->idx >= st->count)
            return 0;
        /* free previous cells */
        if (st->row_texts)
        {
            for (int i = 0; i < 2; i++)
            {
                free(st->row_texts[i]);
                st->row_texts[i] = NULL;
            }
        }
        const VFCol *c = &st->table_ptr->cols[st->idx];
        /* col name */
        st->row_texts[0] = vfdb_strdup(c->name);
        /* type as text */
        const char *tname = (c->type == VF_T_INT) ? "INT" : "TEXT";
        st->row_texts[1] = vfdb_strdup(tname);
        st->idx++;
        return 1;
    }

    case STMT_SELECT_ALL:
    {
        if (st->limit && st->produced >= st->limit)
            return 0;

        /* loop until predicate matches or EOF */
        for (;;)
        {
            /* free previous TEXT cells before loading next row */
            for (int i = 0; i < st->ncols; i++)
            {
                if (st->table_ptr->cols[i].type == VF_T_TEXT && st->row_texts[i])
                {
                    free(st->row_texts[i]);
                    st->row_texts[i] = NULL;
                }
            }
            int ok = heap_scan_next(st->table_ptr, &st->scan, st->row_ints, st->row_texts);
            if (!ok)
                return 0; /* EOF */
            if (row_matches_pred(st))
                break; /* keep row */
        }

        st->produced++;
        return 1;
    }

    case STMT_CREATE_TABLE:
    case STMT_INSERT_VALUES:
        return 0; /* statements produce no rows */

    default:
        return -1;
    }
}

int vfdb_column_count(VFDBStmt *st)
{
    vfdb_log_init_once();
    LOG_DEBUG("vfdb column count stmt %p", (void*)st);
    if (!st)
        return 0;
    switch (st->kind)
    {
    case STMT_SELECT_CONST_INT:
        return 1;
    case STMT_SELECT_ALL:
        return st->ncols;
    case STMT_PRAGMA_SCHEMA:
        return 2;
    case STMT_PRAGMA_TABLES:
        return 1;
    default:
        return 0;
    }
}

vf_type vfdb_column_type(VFDBStmt *st, int i)
{
    vfdb_log_init_once();
    LOG_DEBUG("vfdb column type stmt %p", (void*)st);
    //LOG_DEBUG("vfdb column type int %p", (void*)i);
    if (!st)
        return VF_T_INT;
    switch (st->kind)
    {
    case STMT_SELECT_CONST_INT:
        return VF_T_INT;
    case STMT_SELECT_ALL:
        return st->table_ptr->cols[i].type;
    case STMT_PRAGMA_SCHEMA:
        return VF_T_TEXT;
    case STMT_PRAGMA_TABLES:
        return VF_T_TEXT;
    default:
        return VF_T_INT;
    }
}

int64_t vfdb_column_int(VFDBStmt *st, int i)
{
    vfdb_log_init_once();
    LOG_DEBUG("vfdb column int stmt %p", (void*)st);
    //LOG_DEBUG("vfdb column int int %p", (void*)i);
    if (!st)
        return 0;
    if (st->kind == STMT_SELECT_CONST_INT && i == 0)
        return st->const_int;
    if (st->kind == STMT_SELECT_ALL && st->table_ptr->cols[i].type == VF_T_INT)
        return st->row_ints[i];
    return 0;
}

const char *vfdb_column_text(VFDBStmt *st, int i)
{
    vfdb_log_init_once();
    LOG_DEBUG("*vfdb column text stmt %p", (void*)st);
    //LOG_DEBUG("*vfdb column text int %p", (void*)i);
    if (!st)
        return NULL;
    if (st->kind == STMT_SELECT_ALL && st->table_ptr->cols[i].type == VF_T_TEXT)
        return st->row_texts[i];
    if (st->kind == STMT_PRAGMA_SCHEMA)
        return st->row_texts[i];
    if (st->kind == STMT_PRAGMA_TABLES)
        return st->row_texts ? st->row_texts[0] : NULL;
    return NULL;
}

void vfdb_finalize(VFDBStmt *st)
{
    vfdb_log_init_once();
    LOG_DEBUG("vfdb finalize stmt %p", (void*)st);
    if (!st)
        return;
    if (st->kind == STMT_SELECT_ALL)
    {
        if (st->row_texts)
        {
            for (int i = 0; i < st->ncols; i++)
            {
                if (st->table_ptr && st->table_ptr->cols[i].type == VF_T_TEXT && st->row_texts[i])
                {
                    free(st->row_texts[i]);
                }
            }
        }
        heap_scan_close(&st->scan);
        free(st->row_ints);
        free(st->row_texts);
        if (st->pred.txt)
            free(st->pred.txt);
    }
    else if (st->kind == STMT_PRAGMA_SCHEMA || st->kind == STMT_PRAGMA_TABLES)
    {
        if (st->row_texts)
        {
            /* free any allocated strings we created while stepping */
            int cols = (st->kind == STMT_PRAGMA_SCHEMA) ? 2 : 1;
            for (int i = 0; i < cols; i++)
                if (st->row_texts[i])
                    free(st->row_texts[i]);
            free(st->row_texts);
        }
    }
    free(st);
}

static int pred_match_row(const VFTable *t, Predicate *pred, int64_t *ints, char **txts)
{
    vfdb_log_init_once();
    LOG_DEBUG("pred match row table %p", (void*)t);
    LOG_DEBUG("pred match row pred %p", (void*)pred);
    LOG_DEBUG("pred match row ints %p", (void*)ints);
    LOG_DEBUG("pred match row txts %p", (void*)txts);
    if (!pred || !pred->has)
        return 1;
    int c = pred->col_idx;
    if (pred->is_int)
    {
        if (t->cols[c].type != VF_T_INT)
            return 0;
        return int_cmp(ints[c], pred->i64, pred->op);
    }
    else
    {
        if (t->cols[c].type != VF_T_TEXT)
            return 0;
        return str_cmp(txts[c] ? txts[c] : "", pred->txt ? pred->txt : "", pred->op);
    }
}
