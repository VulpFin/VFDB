# Copyright (C) 2025 TG11
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""
VFDB end-to-end test suite (single file, no pytest needed).

Covers:
- SELECT <int>
- CREATE TABLE, PRAGMA tables/schema
- INSERT, SELECT * (with WHERE and LIMIT)
- UPDATE ... WHERE ...
- DELETE ... WHERE ...
- BEGIN/COMMIT (buffered replay) and ROLLBACK
- Reopen to verify persistence

Usage:
  python full_db_test.py
"""

from pathlib import Path
import os
import sys
import gc
from traceutil import trace, trace_section, trace_exc

# ---------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------
TEST_DIR = Path(__file__).parent / "test"
DB_PATH  = TEST_DIR / "full.vfdb"

# Optional: drive the C logger from the environment (leave unset for quiet)
# os.environ["VFDB_LOG"] = "INFO"   # TRACE | DEBUG | INFO | WARN | ERROR
# os.environ["VFDB_LOG_FILE"] = str(TEST_DIR / "vfdb_c.log")

# ---------------------------------------------------------------------
# Import wrapper
# ---------------------------------------------------------------------
import vfdb


# ---------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------
def run(db, sql):
    """Consume all rows from db.execute(sql) and return a list of tuples."""
    trace(f"SQL >>> {sql!r}")
    rows = []
    for row in db.execute(sql):
        rows.append(row)
    trace(f"OK rows={rows}")
    return rows

#def run_sorted(db, sql, key=None):
#    """Run a query and return rows sorted in Python (default: by column 0)."""
#    rows = run(db, sql)
#    if key is None:
#        key = lambda r: r[0]
#    return sorted(rows, key=key)

def must_equal(got, expect, msg=""):
    if got != expect:
        raise AssertionError(f"{msg}\nExpected: {expect}\nGot:      {got}")

def assert_schema_exact(db, tname, expect_cols):
    """
    expect_cols: list[ (col_name, 'INT'|'TEXT') ] in order
    """
    rows = run(db, f"PRAGMA schema {tname};")
    must_equal(rows, expect_cols,
               f"Catalog/schema mismatch for table {tname!r}.")

def assert_tables(db, expect_names):
    rows = run(db, "PRAGMA tables;")
    must_equal(rows, [(n,) for n in expect_names],
               "PRAGMA tables mismatch.")

def close_db(db):
    """Close the DB handle robustly across wrapper versions."""
    trace(f"Closing DB {db}")
    if hasattr(db, "close") and callable(getattr(db, "close")):
        db.close()
        trace("DB Closed with close()")
        return
    # fallback
    del db
    gc.collect()
    trace("DB references dropped + GC")

def fresh_dir():
    trace("Ensuring clean test dir")
    TEST_DIR.mkdir(parents=True, exist_ok=True)
    # delete old db + aux files
    for p in TEST_DIR.glob("full.vfdb*"):
        try: p.unlink()
        except: pass
    for p in TEST_DIR.glob("full__*.vfheap"):
        try: p.unlink()
        except: pass


# ---------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------
def test_select_const(db):
    with trace_section("SELECT <int>"):
        out = run(db, "SELECT 42;")
        must_equal(out, [(42,)], "SELECT <int> failed")

def test_create_and_schema(db):
    with trace_section("CREATE TABLE + PRAGMA schema"):
        run(db, "CREATE TABLE t(id INT, name TEXT);")
        assert_tables(db, ["t"])
        assert_schema_exact(db, "t", [("id", "INT"), ("name", "TEXT")])

def test_insert_and_select(db):
    with trace_section("INSERT + SELECT *"):
        for i, n in [(1, "a"), (2, "b"), (3, "c")]:
            run(db, f"INSERT INTO t VALUES ({i}, '{n}');")

        rows = run(db, "SELECT * FROM t;")
        must_equal(rows, [(1, "a"), (2, "b"), (3, "c")])

        with trace_section("WHERE id >= 2"):
            rows = run(db, "SELECT * FROM t WHERE id >= 2;")
            must_equal(rows, [(2, "b"), (3, "c")])

        with trace_section("WHERE name = 'b'"):
            rows = run(db, "SELECT * FROM t WHERE name = 'b';")
            must_equal(rows, [(2, "b")])

        with trace_section("WHERE name != 'b'"):
            rows = run(db, "SELECT * FROM t WHERE name != 'b';")
            must_equal(rows, [(1, "a"), (3, "c")])

        with trace_section("LIMIT"):
            rows = run(db, "SELECT * FROM t LIMIT 2;")
            must_equal(rows, [(1, "a"), (2, "b")])

def test_update(db):
    with trace_section("UPDATE SET name='zz' WHERE id >= 2"):
        run(db, "UPDATE t SET name='zz' WHERE id >= 2;")
        rows = run(db, "SELECT * FROM t;")
        must_equal(rows, [(1, "a"), (2, "zz"), (3, "zz")])

def test_delete(db):
    with trace_section("DELETE WHERE name = 'zz'"):
        run(db, "DELETE FROM t WHERE name = 'zz';")
        rows = run(db, "SELECT * FROM t;")
        must_equal(rows, [(1, "a")])

def test_tx_commit(db):
    with trace_section("BEGIN/COMMIT replay"):
        run(db, "BEGIN;")
        run(db, "INSERT INTO t VALUES (2, 'two');")
        run(db, "INSERT INTO t VALUES (3, 'three');")
        run(db, "UPDATE t SET name='AAA' WHERE id = 1;")
        run(db, "COMMIT;")

        #rows = run_sorted(db, "SELECT * FROM t;")
        #must_equal(rows, [(1, "AAA"), (2, "two"), (3, "three")])
        rows = run(db, "SELECT * FROM t;")
        must_equal(rows, [(2, "two"), (3, "three"), (1, "AAA")])

def test_tx_rollback(db):
    with trace_section("BEGIN/ROLLBACK ignores changes"):
        run(db, "BEGIN;")
        run(db, "INSERT INTO t VALUES (4, 'gone');")
        run(db, "UPDATE t SET name='NEVER' WHERE id = 2;")
        run(db, "ROLLBACK;")

        #rows = run_sorted(db, "SELECT * FROM t;")
        #must_equal(rows, [(1, "AAA"), (2, "two"), (3, "three")])
        rows = run(db, "SELECT * FROM t;")
        must_equal(rows, [(2, "two"), (3, "three"), (1, "AAA")])

def test_reopen_and_persist():
    with trace_section("Reopen DB to verify persistence"):
        db = vfdb.Database(str(DB_PATH))
        try:
            assert_tables(db, ["t"])
            assert_schema_exact(db, "t", [("id", "INT"), ("name", "TEXT")])
            #rows = run_sorted(db, "SELECT * FROM t;")
            #must_equal(rows, [(1, "AAA"), (2, "two"), (3, "three")])
            rows = run(db, "SELECT * FROM t;")
            must_equal(rows, [(2, "two"), (3, "three"), (1, "AAA")])
        finally:
            close_db(db)


# ---------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------
if __name__ == "__main__":
    print(f"Python: {sys.version.split()[0]}")
    trace("starting full_db_test")

    fresh_dir()

    # Open fresh DB and run the suite
    db = vfdb.Database(str(DB_PATH))
    trace(f"DB opened at {DB_PATH}")

    try:
        test_select_const(db)
        test_create_and_schema(db)
        test_insert_and_select(db)
        test_update(db)
        test_delete(db)
        test_tx_commit(db)
        test_tx_rollback(db)
    except Exception as e:
        trace_exc(f"TEST FAILED: {e}")
        close_db(db)
        raise
    else:
        close_db(db)

    # Reopen and verify persisted state
    test_reopen_and_persist()

    print("Full DB test passed [ok]")
