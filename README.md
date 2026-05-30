# VFDB

VFDB is a tiny embedded database experiment: a C storage/query core with a
Python facade. It is not SQLite yet, but it is now usable as a small local DB
for project data and experiments.

## What Works

- Open and close database files from Python.
- Persist table metadata in `<db>.meta`.
- Persist table rows in heap files named `<db_base>__<table>.vfheap`.
- `CREATE TABLE` with common PostgreSQL-style scalar type names.
- `INSERT`, `SELECT *`, `WHERE`, `LIMIT`, `UPDATE`, and `DELETE`.
- `BEGIN`, `COMMIT`, and `ROLLBACK` with buffered statement replay.
- `PRAGMA tables;` and `PRAGMA schema <table>;`.
- Python helpers: `connect`, `Database`, `execute`, `fetchone`, `fetchall`,
  `exec`, `executescript`, and simple `?` parameter binding.

## Quick Start

From a checkout, build the native extension first:

```powershell
.\build.bat build
$env:PYTHONPATH = "$PWD\python"
```

```bash
bash build.sh build
export PYTHONPATH="$PWD/python"
```

```python
import vfdb

with vfdb.connect("projects.vfdb") as db:
    db.exec("CREATE TABLE projects(id INT, name TEXT);")
    db.exec("INSERT INTO projects VALUES (?, ?);", [1, "VFDB"])
    rows = db.fetchall("SELECT * FROM projects WHERE id = ?;", [1])
    print(rows)
```

Run the included Python example with:

```powershell
$env:PYTHONPATH = "$PWD\python"
.\.wvenv\Scripts\python.exe examples\python_basic.py
```

```bash
export PYTHONPATH="$PWD/python"
$HOME/.cache/vfdb/venv/bin/python examples/python_basic.py
```

## C API

The C API is SQLite-shaped: open a database, prepare SQL, step rows, read
columns, finalize the statement, then close the database.

```c
VFDB *db = NULL;
vfdb_open("projects.vfdb", &db);

VFDBStmt *st = NULL;
vfdb_prepare(db, "SELECT * FROM projects;", &st);
while (vfdb_step(st) == 1) {
    printf("%lld %s\n",
           (long long)vfdb_column_int(st, 0),
           vfdb_column_text(st, 1));
}
vfdb_finalize(st);

vfdb_close(db);
```

The repo includes [examples/c_basic.c](examples/c_basic.c). On WSL/Linux you
can compile it directly against the core sources:

```bash
gcc -Iinclude -Isrc \
  examples/c_basic.c \
  src/bptree.c src/catalog.c src/exec.c src/fileio.c src/heap.c \
  src/parser.c src/tx.c src/util.c src/vf_type.c src/vfdb.c src/vfdb_log.c \
  -o examples/c_basic
./examples/c_basic
```

## Supported SQL Subset

```sql
CREATE TABLE tasks(id INT, title TEXT);
INSERT INTO tasks VALUES (1, 'write docs');
SELECT * FROM tasks;
SELECT * FROM tasks WHERE id >= 1 LIMIT 10;
UPDATE tasks SET title='done' WHERE id = 1;
DELETE FROM tasks WHERE title = 'done';
BEGIN;
COMMIT;
ROLLBACK;
PRAGMA tables;
PRAGMA schema tasks;
```

## Type Support

VFDB accepts common PostgreSQL-style scalar type names and aliases. Storage is
grouped into a few practical classes:

```text
INT      SMALLINT, INT, INTEGER, BIGINT, SERIAL, BIGSERIAL, INT2/4/8
REAL     REAL, FLOAT, DOUBLE PRECISION, NUMERIC, DECIMAL, MONEY
BOOL     BOOL, BOOLEAN
BYTEA    BYTEA, BLOB, BYTES, BINARY
TEXT     TEXT, VARCHAR(n), CHARACTER VARYING(n), CHAR(n), NAME, XML
DATE     DATE
TIME     TIME, TIME WITH/WITHOUT TIME ZONE
TIMESTAMP TIMESTAMP, TIMESTAMPTZ, TIMESTAMP WITH/WITHOUT TIME ZONE
INTERVAL INTERVAL
JSONB    JSON, JSONB
UUID     UUID
INET     INET, CIDR
MACADDR  MACADDR, MACADDR8
```

Python returns `INT` as `int`, `REAL` as `float`, `BOOL` as `bool`, `BYTEA` as
`bytes`, and the date/time/json/uuid/network families as strings for now.

Current limits are intentional and visible: no joins, no indexes in the query
planner, no NULL values, no arrays/ranges/user-defined types, and no
PostgreSQL-grade date/time or arbitrary-precision numeric operations yet.

## Development

Use the OS wrapper for your shell:

```powershell
.\build.bat all
```

```bash
./build.sh all
```

Both wrappers support:

```text
deps   install build/test Python dependencies
build  compile the native extension in place
test   run the pytest suite
smoke  run the larger end-to-end script
wheel  build a wheel into dist/
clean  remove generated build/test artifacts
all    build, test, and smoke-test
```

On WSL/Ubuntu, `build.sh` creates and uses `$HOME/.cache/vfdb/venv`
automatically. This avoids Python `venv` issues on Windows-mounted paths like
`/mnt/u`. If `python3-venv` or build tools are missing, install:

```bash
sudo apt update && sudo apt install -y python3 python3-venv python3-dev build-essential
```

The root `.gitignore` keeps generated wheels, build folders, native binaries,
VFDB runtime files, local venvs, and the local `.lu` credential note out of
source control.
