# Copyright (C) 2025 TG11
# GNU AGPL v3-or-later
from __future__ import annotations

try:
    from importlib.metadata import version as _pkg_version

    __version__ = _pkg_version("vfdb")
except Exception:
    __version__ = "0+unknown"

from ._native import finalize, open_raw, prepare, step

try:
    from ._native import close_raw
except Exception:
    close_raw = None


class VFDBError(Exception):
    """Base error for vfdb Python bindings."""


def _sql_literal(value):
    if value is None:
        raise VFDBError("VFDB does not support NULL values yet")
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return repr(value)
    if isinstance(value, (bytes, bytearray, memoryview)):
        return "X'" + bytes(value).hex() + "'"
    return "'" + str(value).replace("'", "''") + "'"


def _bind_params(sql: str, params=None) -> str:
    if params is None:
        return sql

    values = list(params)
    out = []
    value_idx = 0
    in_quote = False
    i = 0
    while i < len(sql):
        ch = sql[i]
        if ch == "'":
            out.append(ch)
            if in_quote and i + 1 < len(sql) and sql[i + 1] == "'":
                out.append(sql[i + 1])
                i += 2
                continue
            in_quote = not in_quote
        elif ch == "?" and not in_quote:
            if value_idx >= len(values):
                raise VFDBError("not enough parameters supplied")
            out.append(_sql_literal(values[value_idx]))
            value_idx += 1
        else:
            out.append(ch)
        i += 1

    if value_idx != len(values):
        raise VFDBError("too many parameters supplied")
    return "".join(out)


class Database:
    """
    High-level connection facade over the native stepping API.

    `execute(sql, params)` yields tuples per row. `exec(sql, params)` runs
    statements and discards rows. Use `with db.transaction(): ...` for an
    explicit transaction.
    """

    def __init__(self, path: str):
        self._db = open_raw(path)
        self._closed = False

    def _ensure_open(self):
        if self._closed:
            raise VFDBError("database is closed")

    def execute(self, sql: str, params=None):
        """Yield rows as tuples from a SQL statement."""
        self._ensure_open()
        sql = _bind_params(sql, params)
        st = prepare(self._db, sql)
        try:
            while True:
                result = step(st)
                if result is False:
                    break
                has_row, row = result
                if has_row:
                    yield row
        finally:
            finalize(st)

    def fetchall(self, sql: str, params=None):
        """Return all rows as a list of tuples."""
        return list(self.execute(sql, params))

    def fetchone(self, sql: str, params=None):
        """Return the first row or None."""
        for row in self.execute(sql, params):
            return row
        return None

    def exec(self, sql: str, params=None):
        """Execute a statement and discard any rows."""
        for _ in self.execute(sql, params):
            pass

    def executescript(self, sql_script: str):
        """Run a semicolon-separated SQL script, respecting quoted semicolons."""
        buf = []
        in_quote = False
        i = 0
        while i < len(sql_script):
            ch = sql_script[i]
            buf.append(ch)
            if ch == "'":
                if in_quote and i + 1 < len(sql_script) and sql_script[i + 1] == "'":
                    buf.append(sql_script[i + 1])
                    i += 2
                    continue
                in_quote = not in_quote
            if ch == ";" and not in_quote:
                stmt = "".join(buf).strip()
                if stmt:
                    self.exec(stmt)
                buf.clear()
            i += 1
        tail = "".join(buf).strip()
        if tail:
            self.exec(tail)

    def begin(self):
        self.exec("BEGIN;")

    def commit(self):
        self.exec("COMMIT;")

    def rollback(self):
        self.exec("ROLLBACK;")

    def transaction(self):
        return _Transaction(self)

    def close(self):
        if self._closed:
            return
        if close_raw is not None:
            close_raw(self._db)
        self._closed = True

    def __enter__(self):
        self._ensure_open()
        return self

    def __exit__(self, exc_type, exc, tb):
        try:
            if exc_type is None:
                try:
                    self.commit()
                except Exception:
                    pass
            else:
                try:
                    self.rollback()
                except Exception:
                    pass
        finally:
            self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


class _Transaction:
    def __init__(self, db: Database):
        self._db = db

    def __enter__(self):
        try:
            self._db.begin()
        except Exception as e:
            raise VFDBError(f"BEGIN failed: {e}") from e
        return self._db

    def __exit__(self, exc_type, exc, tb):
        if exc_type is None:
            try:
                self._db.commit()
            except Exception as e:
                raise VFDBError(f"COMMIT failed: {e}") from e
        else:
            try:
                self._db.rollback()
            except Exception:
                pass


def connect(path: str) -> Database:
    """Open a database connection."""
    return Database(path)


class raw:
    """Explicit access to the native API."""

    open = staticmethod(open_raw)
    prepare = staticmethod(prepare)
    step = staticmethod(step)
    finalize = staticmethod(finalize)
    close = staticmethod(close_raw) if close_raw is not None else None


DB = Database

__all__ = [
    "__version__",
    "VFDBError",
    "Database",
    "DB",
    "connect",
    "raw",
    "prepare",
    "step",
    "finalize",
    "open_raw",
    "close_raw",
]
