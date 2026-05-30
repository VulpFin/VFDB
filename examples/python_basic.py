from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

import vfdb


db_path = ROOT / "examples" / "projects.vfdb"
for path in [
    db_path,
    Path(str(db_path) + ".meta"),
    ROOT / "examples" / "projects__projects.vfheap",
]:
    path.unlink(missing_ok=True)

with vfdb.connect(str(db_path)) as db:
    db.exec("CREATE TABLE projects(id INT, name TEXT);")
    db.exec("INSERT INTO projects VALUES (?, ?);", [1, "VFDB"])
    db.exec("INSERT INTO projects VALUES (?, ?);", [2, "Tiny app DB"])

    print(db.fetchall("PRAGMA tables;"))
    print(db.fetchall("PRAGMA schema projects;"))
    print(db.fetchall("SELECT * FROM projects WHERE id >= ? LIMIT 10;", [1]))
