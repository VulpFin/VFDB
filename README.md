# VFDB

VFDB is a tiny embedded database experiment: a C storage/query core with a
Python facade. It is not SQLite yet, but it is now usable as a small local DB
for project data and experiments.

## What Works

- Open and close database files from Python.
- Persist table metadata in `<db>.meta`.
- Persist table rows in heap files named `<db_base>__<table>.vfheap`.
- `CREATE TABLE` with `INT` and `TEXT` columns.
- `INSERT`, `SELECT *`, `WHERE`, `LIMIT`, `UPDATE`, and `DELETE`.
- `BEGIN`, `COMMIT`, and `ROLLBACK` with buffered statement replay.
- `PRAGMA tables;` and `PRAGMA schema <table>;`.
- Python helpers: `connect`, `Database`, `execute`, `fetchone`, `fetchall`,
  `exec`, `executescript`, and simple `?` parameter binding.

## Quick Start

```python
import vfdb

with vfdb.connect("projects.vfdb") as db:
    db.exec("CREATE TABLE projects(id INT, name TEXT);")
    db.exec("INSERT INTO projects VALUES (?, ?);", [1, "VFDB"])
    rows = db.fetchall("SELECT * FROM projects WHERE id = ?;", [1])
    print(rows)
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

Current limits are intentional and visible: no joins, no indexes in the query
planner, no NULL values, and only persisted `INT`/`TEXT` values are supported
by the storage path.

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
