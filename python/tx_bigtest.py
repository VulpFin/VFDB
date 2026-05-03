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

# tx_bigtest.py
# Quick end-to-end test for VFDB: PRAGMA, WHERE ops, UPDATE/DELETE, BEGIN/COMMIT/ROLLBACK

import os, sys, vfdb

print(f"Python: {sys.version.split()[0]}")

# Put artifacts in a predictable place
try:
    os.chdir(r"U:\Projects\VFDB\python\test")
except Exception:
    pass

DB_NAME = "ops.vfdb"

# Clean previous run junk (ok if missing)
for extra in [
    DB_NAME,
    DB_NAME + ".meta",
    "ops__t.vfheap",          # heap for table t
    "ops__u.vfheap",          # heap for table u (if you add more)
]:
    try:
        os.remove(extra)
    except FileNotFoundError:
        pass

db = vfdb.Database(DB_NAME)
print(f"Opened {DB_NAME} [ok]")

def run(sql):
    rows = list(db.execute(sql))
    print(f"{sql.strip()} -> {rows}")
    return rows

# 0) Constant select
run("SELECT 7;")

# 1) Create & seed
run("CREATE TABLE t(id INT, name TEXT);")
run("INSERT INTO t VALUES (1,'a');")
run("INSERT INTO t VALUES (2,'b');")
run("INSERT INTO t VALUES (3,'c');")
run("INSERT INTO t VALUES (4,'d');")

print("\n-- PRAGMA sanity --")
print("PRAGMA tables;")
for row in db.execute("PRAGMA tables;"):
    print("  table:", row)

print("PRAGMA schema t;")
for row in db.execute("PRAGMA schema t;"):
    print("  col:", row)

print("\n-- Basic SELECTs --")
run("SELECT * FROM t;")
run("SELECT * FROM t WHERE id >= 2;")
run("SELECT * FROM t WHERE id != 3;")
run("SELECT * FROM t WHERE name = 'b';")
run("SELECT * FROM t WHERE name != 'zzz';")
run("SELECT * FROM t WHERE id > 1 LIMIT 2;")

print("\n-- UPDATE in a transaction (id > 2) --")
run("BEGIN;")
run("UPDATE t SET name = 'xx' WHERE id > 2;")
# also delete one row to test tombstones
run("DELETE FROM t WHERE id = 1;")
run("COMMIT;")

print("\nAfter COMMIT:")
after_commit = run("SELECT * FROM t;")

# Expect: id=1 deleted; id=3,4 name='xx'; id=2 still 'b'
# We can't ORDER BY yet, so just quick checks by membership:
def as_set(rows): return set(rows)
assert (2, 'b') in as_set(after_commit)
assert (3, 'xx') in as_set(after_commit)
assert (4, 'xx') in as_set(after_commit)
assert (1, 'a') not in as_set(after_commit)
print("Post-commit checks [ok]")

print("\n-- ROLLBACK test: try to change id=2 but rollback --")
run("BEGIN;")
run("UPDATE t SET name = 'rollback' WHERE id = 2;")
run("ROLLBACK;")

after_rollback = run("SELECT * FROM t;")
assert (2, 'b') in as_set(after_rollback), "ROLLBACK failed: id=2 should still be 'b'"
print("Rollback checks [ok]")

print("\n-- DELETE with text predicate (keep only rows with name = 'xx') --")
run("DELETE FROM t WHERE name != 'xx';")
after_delete = run("SELECT * FROM t;")
# By now only the rows we updated to 'xx' should remain (ids 3 and 4)
assert as_set(after_delete) == {(3, 'xx'), (4, 'xx')}
print("Text != delete checks [ok]")

print("\nAll tests passed [ok]")
del db
