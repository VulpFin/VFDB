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

from pathlib import Path
import sys

import vfdb


print(f"Python: {sys.version.split()[0]}")

test_dir = Path(__file__).parent / "test"
test_dir.mkdir(parents=True, exist_ok=True)
db_path = test_dir / "test.vfdb"
for path in [db_path, db_path.with_suffix(".vfdb.meta"), test_dir / "test__users.vfheap"]:
    try:
        path.unlink()
    except FileNotFoundError:
        pass

with vfdb.Database(str(db_path)) as db:
    print("Opened test.vfdb [ok]")
    db.exec("CREATE TABLE users(id INT, name TEXT);")
    print("Create table users(id, name) in db test.vfdb [ok]")
    db.exec("INSERT INTO users VALUES (1,'Solye');")
    db.exec("INSERT INTO users VALUES (2,'Gage');")
    db.exec("INSERT INTO users VALUES (3,'Nyx');")
    print("Insert data into table users in db test.vfdb [ok]")
    rows = db.fetchall("SELECT * FROM users;")
    assert rows == [(1, "Solye"), (2, "Gage"), (3, "Nyx")], rows
    print("Full select:", rows)
    print("Limit 2:", db.fetchall("SELECT * FROM users LIMIT 2;"))
    print("Const select:", db.fetchall("SELECT 7;"))

print("Test Done, Passed!")
