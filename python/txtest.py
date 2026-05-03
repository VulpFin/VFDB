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

import vfdb, os
p = r"U:\Projects\VFDB\python\test\ops.vfdb"
db = vfdb.Database(p)
list(db.execute("CREATE TABLE t(id INT, name TEXT);"))
list(db.execute("INSERT INTO t VALUES (1,'a');"))
list(db.execute("INSERT INTO t VALUES (2,'b');"))
list(db.execute("INSERT INTO t VALUES (3,'b');"))

# SELECT with ops
print(list(db.execute("SELECT * FROM t WHERE id >= 2;")))   # expect (2,'b'), (3,'b')
print(list(db.execute("SELECT * FROM t WHERE name != 'a';")))# expect two rows

# UPDATE with op
list(db.execute("UPDATE t SET name='xx' WHERE id > 2;"))
print(list(db.execute("SELECT * FROM t;")))                  # id=3 should be 'xx'

# DELETE with op
list(db.execute("DELETE FROM t WHERE id != 2;"))
print(list(db.execute("SELECT * FROM t;")))                  # only id=2 row remains
