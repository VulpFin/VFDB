# python/tx_reopen_test.py

import sys, os, gc
from pathlib import Path
from traceutil import trace, trace_exc

print(f"Python: {sys.version.split()[0]}")
trace("starting tx_reopen_test")

import vfdb

DB_PATH = Path(__file__).parent / "test" / "ops.vfdb"
CAT_PATH = DB_PATH.with_suffix(".vfdb.meta")          # e.g. ops.vfdb.meta
HEAP_PATH = DB_PATH.parent / (DB_PATH.stem + "__t.vfheap")  # e.g. ops__t.vfheap

# -----------------------------------------------------------------------------
# helpers
# -----------------------------------------------------------------------------

def run_debug(db, sql):
    trace(f"SQL >>> {sql!r}")
    try:
        rows = list(db.execute(sql))
        trace(f"OK, rows={rows}")
        return rows
    except Exception as e:
        trace_exc(f"FAIL: {e}")
        raise

def close_db(db):
    trace(f"Closing DB {db}")
    if hasattr(db, "close") and callable(db.close):
        db.close()
        trace("DB Closed with close()")
        return
    native = getattr(vfdb, "_native", None)
    if native and hasattr(native, "close_raw"):
        try:
            native.close_raw(db._db)
            trace("DB Closed with native.close_raw()")
            return
        except Exception as e:
            trace_exc(f"close_raw failed: {e}")
    del db
    gc.collect()
    trace("DB reference dropped, GC collected")

def hexdump(path: Path, n_head=256, n_tail=256):
    if not path.exists():
        trace(f"[hexdump] {path} (missing)")
        return
    try:
        data = path.read_bytes()
    except Exception as e:
        trace_exc(f"[hexdump] read failed for {path}: {e}")
        return
    L = len(data)
    head = data[: min(n_head, L)]
    tail = data[max(0, L - n_tail):]
    def fmt(blob):
        return " ".join(f"{b:02x}" for b in blob)
    trace(f"[hexdump] {path.name} size={L}")
    trace(f"[hexdump] HEAD({len(head)}): {fmt(head)}")
    if L > n_head:
        trace(f"[hexdump] ...")
    trace(f"[hexdump] TAIL({len(tail)}): {fmt(tail)}")

def assert_schema_exact(db, table, expected):
    got = run_debug(db, f"PRAGMA schema {table};")
    if got != expected:
        # dump extra context to help fix catalog (cat_add_table/save/load)
        trace("PRAGMA tables -> " + repr(run_debug(db, "PRAGMA tables;")))
        trace("!!! Catalog/schema mismatch detected after reopen.")
        trace(f"Expected: {expected}")
        trace(f"Got:      {got}")
        # on-disk bytes to help diagnose catalog loader/writer
        hexdump(CAT_PATH)
        hexdump(HEAP_PATH)
        raise RuntimeError(
            "Catalog/schema mismatch.\n"
            f"  Expected: {expected}\n"
            f"  Got:      {got}\n\n"
            "This means the catalog is not preserving column names correctly.\n"
            "Check cat_add_table()/cat_save()/cat_load() so that each VFCol.name "
            "is stored and reloaded. You likely lost the second column's name."
        )

# -----------------------------------------------------------------------------
# fresh start
# -----------------------------------------------------------------------------

trace("Ensuring clean test dir")
DB_PATH.parent.mkdir(parents=True, exist_ok=True)
for p in [DB_PATH, CAT_PATH, HEAP_PATH]:
    if p.exists():
        try:
            p.unlink()
        except Exception:
            pass

# -----------------------------------------------------------------------------
# 1) open, create, seed
# -----------------------------------------------------------------------------

db = vfdb.Database(str(DB_PATH))
trace(f"DB {db} created")

# quick sanity
run_debug(db, "SELECT 7;")

# create and verify schema
run_debug(db, "CREATE TABLE t(id INT, name TEXT);")
assert_schema_exact(db, "t", [("id", "INT"), ("name", "TEXT")])

# seed rows
for i, n in [(1, "a"), (2, "b"), (3, "c")]:
    run_debug(db, f"INSERT INTO t VALUES ({i}, '{n}');")

seed = run_debug(db, "SELECT * FROM t;")
print("seed ->", seed)
if seed != [(1, "a"), (2, "b"), (3, "c")]:
    raise AssertionError(f"seed mismatch: {seed}")

close_db(db)

# -----------------------------------------------------------------------------
# 2) reopen, verify catalog survived, then UPDATE
# -----------------------------------------------------------------------------

trace("Reopening DB")
db = vfdb.Database(str(DB_PATH))

# must still be there
assert_schema_exact(db, "t", [("id", "INT"), ("name", "TEXT")])

# run the UPDATE that previously failed if the name vanished
run_debug(db, "UPDATE t SET name='zz' WHERE id >= 2;")

after = run_debug(db, "SELECT * FROM t;")
print("after update ->", after)
expected = [(1, "a"), (2, "zz"), (3, "zz")]
if after != expected:
    raise AssertionError(f"mismatch: {after} != {expected}")

close_db(db)
print("Reopen/update test passed [ok]")
