from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

import vfdb


def run(db, sql):
    return list(db.execute(sql))


def test_crud_transactions_and_persistence(tmp_path):
    db_path = tmp_path / "app.vfdb"

    with vfdb.connect(str(db_path)) as db:
        assert run(db, "SELECT 7;") == [(7,)]
        assert run(db, "CREATE TABLE tasks(id INT, title TEXT);") == []
        assert run(db, "PRAGMA tables;") == [("tasks",)]
        assert run(db, "PRAGMA schema tasks;") == [("id", "INT"), ("title", "TEXT")]

        run(db, "INSERT INTO tasks VALUES (1, 'write');")
        db.exec("INSERT INTO tasks VALUES (?, ?);", [2, "ship"])
        db.exec("INSERT INTO tasks VALUES (?, ?);", [3, "O'Brien"])
        assert db.fetchall("SELECT * FROM tasks WHERE id = ?;", [2]) == [(2, "ship")]
        assert db.fetchone("SELECT * FROM tasks WHERE title = ?;", ["O'Brien"]) == (3, "O'Brien")

        run(db, "UPDATE tasks SET title='done' WHERE id = 2;")
        assert run(db, "SELECT * FROM tasks WHERE id != 3;") == [(1, "write"), (2, "done")]

        run(db, "BEGIN;")
        run(db, "INSERT INTO tasks VALUES (3, 'tmp');")
        run(db, "ROLLBACK;")
        assert run(db, "SELECT * FROM tasks WHERE id != 3;") == [(1, "write"), (2, "done")]

        run(db, "BEGIN;")
        run(db, "DELETE FROM tasks WHERE id = 1;")
        run(db, "COMMIT;")
        assert run(db, "SELECT * FROM tasks WHERE id != 3;") == [(2, "done")]

    with vfdb.connect(str(db_path)) as db:
        assert run(db, "PRAGMA tables;") == [("tasks",)]
        assert run(db, "SELECT * FROM tasks WHERE id != 3;") == [(2, "done")]
        assert db.fetchone("SELECT * FROM tasks WHERE title = ?;", ["O'Brien"]) == (3, "O'Brien")


def test_postgres_style_scalar_types(tmp_path):
    db_path = tmp_path / "types.vfdb"

    with vfdb.connect(str(db_path)) as db:
        db.exec(
            """
            CREATE TABLE typed(
                id BIGSERIAL,
                ok BOOLEAN,
                score DOUBLE PRECISION,
                payload BYTEA,
                info JSONB,
                created TIMESTAMP WITH TIME ZONE,
                day DATE,
                host INET,
                ident UUID,
                label VARCHAR(40)
            );
            """
        )
        assert db.fetchall("PRAGMA schema typed;") == [
            ("id", "INT"),
            ("ok", "BOOL"),
            ("score", "REAL"),
            ("payload", "BYTEA"),
            ("info", "JSONB"),
            ("created", "TIMESTAMP"),
            ("day", "DATE"),
            ("host", "INET"),
            ("ident", "UUID"),
            ("label", "TEXT"),
        ]

        db.exec(
            "INSERT INTO typed VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            [
                1,
                True,
                12.5,
                b"vf",
                '{"kind":"demo"}',
                "2026-05-03T12:30:00Z",
                "2026-05-03",
                "127.0.0.1",
                "123e4567-e89b-12d3-a456-426614174000",
                "hello",
            ],
        )

        row = db.fetchone("SELECT * FROM typed WHERE score >= ?;", [12.0])
        assert row == (
            1,
            True,
            12.5,
            b"vf",
            '{"kind":"demo"}',
            "2026-05-03T12:30:00Z",
            "2026-05-03",
            "127.0.0.1",
            "123e4567-e89b-12d3-a456-426614174000",
            "hello",
        )

        db.exec("UPDATE typed SET ok=FALSE WHERE id = 1;")
        assert db.fetchone("SELECT * FROM typed WHERE ok = FALSE;")[1] is False

    with vfdb.connect(str(db_path)) as db:
        assert db.fetchone("SELECT * FROM typed WHERE id = 1;")[3] == b"vf"
