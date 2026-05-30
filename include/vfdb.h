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

/* src/vfdb.h */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct VFDB VFDB; /* opaque handle */
    typedef struct VFDBStmt VFDBStmt;

    typedef enum
    {
        VFDB_OK = 0,
        VFDB_ERR = 1,
        VFDB_USSQL = 2
    } vfdb_rc;

    typedef enum {
        VF_T_INT        = 1,
        VF_T_TEXT       = 2,
        VF_T_REAL       = 3,
        VF_T_BOOL       = 4,
        VF_T_BLOB       = 5,
        VF_T_DATETIME   = 6,
        VF_T_DATE       = 7,
        VF_T_TIME       = 8,
        VF_T_TIMESTAMP  = 9,
        VF_T_INTERVAL   = 10,
        VF_T_JSON       = 11,
        VF_T_UUID       = 12,
        VF_T_INET       = 13,
        VF_T_MACADDR    = 14,
    } vf_type;

    /* Open/close */
    vfdb_rc vfdb_open(const char *path, VFDB **out_db);
    void    vfdb_close(VFDB *db);

    /* VFQL prepare/step/finalize (SQLite-style) */
    vfdb_rc     vfdb_prepare(VFDB *db, const char *vfql, VFDBStmt **out_stmt);
    int         vfdb_step(VFDBStmt *st); /* 1=row, 0=done, <0=err */
    int64_t     vfdb_column_int(VFDBStmt *st, int i);
    double      vfdb_column_real(VFDBStmt *st, int i);
    const char* vfdb_column_text(VFDBStmt *st, int i);
    int         vfdb_column_count(VFDBStmt *st);
    vf_type     vfdb_column_type(VFDBStmt *st, int i);
    void        vfdb_finalize(VFDBStmt *st);

    /* Transactions */
    vfdb_rc vfdb_begin(VFDB *db);
    vfdb_rc vfdb_commit(VFDB *db);
    vfdb_rc vfdb_rollback(VFDB *db);

    /* Python UDFs: register a named Python function body */
    vfdb_rc vfdb_py_create_function(VFDB *db,
                                    const char *name, const char *args_sig, const char *ret_sig,
                                    const char *py_src /* function body: `def f(...): ...` */);

#ifdef __cplusplus
}
#endif
