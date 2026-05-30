#include <stdio.h>
#include <stdlib.h>

#include "vfdb.h"


static void die_if(vfdb_rc rc, const char *what)
{
    if (rc != VFDB_OK)
    {
        fprintf(stderr, "%s failed\n", what);
        exit(1);
    }
}

static void exec_sql(VFDB *db, const char *sql)
{
    VFDBStmt *st = NULL;
    die_if(vfdb_prepare(db, sql, &st), sql);

    int rc = 0;
    while ((rc = vfdb_step(st)) == 1)
    {
        /* Ignore rows for statements used as commands. */
    }

    vfdb_finalize(st);

    if (rc < 0)
    {
        fprintf(stderr, "step failed for: %s\n", sql);
        exit(1);
    }
}

static void print_query(VFDB *db, const char *sql)
{
    VFDBStmt *st = NULL;
    die_if(vfdb_prepare(db, sql, &st), sql);

    int rc = 0;
    while ((rc = vfdb_step(st)) == 1)
    {
        int ncols = vfdb_column_count(st);
        for (int i = 0; i < ncols; i++)
        {
            if (i > 0)
                printf(" | ");

            if (vfdb_column_type(st, i) == VF_T_INT)
                printf("%lld", (long long)vfdb_column_int(st, i));
            else
                printf("%s", vfdb_column_text(st, i));
        }
        printf("\n");
    }

    vfdb_finalize(st);

    if (rc < 0)
    {
        fprintf(stderr, "query failed for: %s\n", sql);
        exit(1);
    }
}

int main(void)
{
    VFDB *db = NULL;
    remove("examples/projects_c.vfdb");
    remove("examples/projects_c.vfdb.meta");
    remove("examples/projects_c__projects.vfheap");

    die_if(vfdb_open("examples/projects_c.vfdb", &db), "open");

    exec_sql(db, "CREATE TABLE projects(id INT, name TEXT);");
    die_if(vfdb_begin(db), "begin");
    exec_sql(db, "INSERT INTO projects VALUES (1, 'VFDB');");
    exec_sql(db, "INSERT INTO projects VALUES (2, 'C API');");
    die_if(vfdb_commit(db), "commit");

    print_query(db, "PRAGMA tables;");
    print_query(db, "PRAGMA schema projects;");
    print_query(db, "SELECT * FROM projects WHERE id >= 1 LIMIT 10;");

    vfdb_close(db);
    return 0;
}
