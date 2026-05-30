/**
 * Copyright (C) 2025 TG11
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* python/vfdb/_native.c - minimal working CPython C-extension wrapper */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>
#include "vfdb.h"
#include "vfdb_log.h"

static const char *CAPSULE_DB = "VFDB*";
static const char *CAPSULE_ST = "VFDBStmt*";

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

static PyObject *bytes_from_hex(const char *hex)
{
    Py_ssize_t hex_len = hex ? (Py_ssize_t)strlen(hex) : 0;
    if ((hex_len % 2) != 0)
        return PyBytes_FromStringAndSize("", 0);

    PyObject *out = PyBytes_FromStringAndSize(NULL, hex_len / 2);
    if (!out)
        return NULL;
    char *buf = PyBytes_AS_STRING(out);
    for (Py_ssize_t i = 0; i < hex_len; i += 2)
    {
        int hi = hex_value(hex[i]);
        int lo = hex_value(hex[i + 1]);
        if (hi < 0 || lo < 0)
        {
            Py_DECREF(out);
            return PyBytes_FromStringAndSize("", 0);
        }
        buf[i / 2] = (char)((hi << 4) | lo);
    }
    return out;
}

static void db_capsule_destructor(PyObject *capsule)
{
    vfdb_log_init_once();
    LOG_DEBUG("db capsule descructor capsule %p", (void*)capsule);
    VFDB *db = (VFDB *)PyCapsule_GetPointer(capsule, CAPSULE_DB);
    if (db)
        vfdb_close(db);
}

/* open_raw(path:str) -> capsule(VFDB*) */
static PyObject *py_open_raw(PyObject *self, PyObject *args)
{
    vfdb_log_init_once();
    LOG_DEBUG("*py open raw self %p", (void*)self);
    LOG_DEBUG("*py open raw args %p", (void*)args);
    (void)self;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "s", &path))
        return NULL;

    VFDB *db = NULL;
    if (vfdb_open(path, &db) != VFDB_OK || db == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "vfdb_open failed");
        return NULL;
    }
    /* capsule will just carry the pointer for now; add a destructor later */
    return PyCapsule_New((void *)db, CAPSULE_DB, db_capsule_destructor);
}

/* close_raw(db_capsule) -> None */
static PyObject *py_close_raw(PyObject *self, PyObject *args)
{
    vfdb_log_init_once();
    LOG_DEBUG("*py close raw self %p", (void*)self);
    LOG_DEBUG("*py close raw args %p", (void*)args);
    (void)self;
    PyObject *dbc = NULL;
    if (!PyArg_ParseTuple(args, "O", &dbc))
        return NULL;

    VFDB *db = (VFDB *)PyCapsule_GetPointer(dbc, CAPSULE_DB);
    if (!db)
        return NULL;

    vfdb_close(db);
    PyCapsule_SetDestructor(dbc, NULL);
    PyCapsule_SetName(dbc, "VFDB.closed");
    Py_RETURN_NONE;
}

/* prepare(db_capsule, sql) -> stmt_capsule */
static PyObject *py_prepare(PyObject *self, PyObject *args)
{
    vfdb_log_init_once();
    LOG_DEBUG("*py prepare self %p", (void*)self);
    LOG_DEBUG("*py prepare args %p", (void*)args);
    (void)self;
    PyObject *dbc = NULL;
    const char *sql = NULL;
    if (!PyArg_ParseTuple(args, "Os", &dbc, &sql))
        return NULL;
    VFDB *db = (VFDB *)PyCapsule_GetPointer(dbc, CAPSULE_DB);
    if (!db)
        return NULL;

    VFDBStmt *st = NULL;
    if (vfdb_prepare(db, sql, &st) != VFDB_OK)
    {
        PyErr_SetString(PyExc_RuntimeError, "vfdb_prepare failed");
        return NULL;
    }
    return PyCapsule_New((void *)st, CAPSULE_ST, NULL);
}

/* step(stmt_capsule) -> (has_row:bool, row:tuple|None) */
static PyObject *py_step(PyObject *self, PyObject *args)
{
    vfdb_log_init_once();
    LOG_DEBUG("*py step self %p", (void*)self);
    LOG_DEBUG("*py step args %p", (void*)args);
    (void)self;
    PyObject *stc = NULL;
    if (!PyArg_ParseTuple(args, "O", &stc))
        return NULL;
    VFDBStmt *st = (VFDBStmt *)PyCapsule_GetPointer(stc, "VFDBStmt*");
    if (!st)
        return NULL;

    int rc = vfdb_step(st);
    if (rc < 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "vfdb_step error");
        return NULL;
    }
    if (rc == 0)
        Py_RETURN_FALSE;

    int n = vfdb_column_count(st);
    PyObject *row = PyTuple_New(n);
    for (int i = 0; i < n; i++)
    {
        vf_type t = vfdb_column_type(st, i);
        if (t == VF_T_INT)
        {
            PyTuple_SET_ITEM(row, i, PyLong_FromLongLong(vfdb_column_int(st, i)));
        }
        else if (t == VF_T_BOOL)
        {
            PyObject *b = vfdb_column_int(st, i) ? Py_True : Py_False;
            Py_INCREF(b);
            PyTuple_SET_ITEM(row, i, b);
        }
        else if (t == VF_T_REAL)
        {
            PyTuple_SET_ITEM(row, i, PyFloat_FromDouble(vfdb_column_real(st, i)));
        }
        else if (t == VF_T_BLOB)
        {
            const char *s = vfdb_column_text(st, i);
            PyTuple_SET_ITEM(row, i, bytes_from_hex(s ? s : ""));
        }
        else
        {
            const char *s = vfdb_column_text(st, i);
            PyTuple_SET_ITEM(row, i, PyUnicode_FromString(s ? s : ""));
        }
    }
    return Py_BuildValue("(OO)", Py_True, row);
}

/* py_create_function(db_capsule, name, args_sig, ret_sig, src) -> None */
static PyObject *py_py_create_function(PyObject *self, PyObject *args)
{
    vfdb_log_init_once();
    LOG_DEBUG("*py py create function self %p", (void*)self);
    LOG_DEBUG("*py py create function args %p", (void*)args);
    (void)self;
    PyObject *capsule = NULL;
    const char *name = NULL, *asig = NULL, *rsig = NULL, *src = NULL;

    if (!PyArg_ParseTuple(args, "Ossss", &capsule, &name, &asig, &rsig, &src))
        return NULL;

    VFDB *db = (VFDB *)PyCapsule_GetPointer(capsule, CAPSULE_DB);
    if (db == NULL)
        return NULL; /* PyCapsule_GetPointer already set an error */

    if (vfdb_py_create_function(db, name, asig, rsig, src) != VFDB_OK)
    {
        PyErr_SetString(PyExc_RuntimeError, "create_function failed");
        return NULL;
    }
    Py_RETURN_NONE;
}

/* finalize(stmt_capsule) -> None */
static PyObject *py_finalize(PyObject *self, PyObject *args)
{
    vfdb_log_init_once();
    LOG_DEBUG("*py finalize self %p", (void*)self);
    LOG_DEBUG("*py finalize args %p", (void*)args);
    (void)self;
    PyObject *stc = NULL;
    if (!PyArg_ParseTuple(args, "O", &stc))
        return NULL;
    VFDBStmt *st = (VFDBStmt *)PyCapsule_GetPointer(stc, CAPSULE_ST);
    if (st)
        vfdb_finalize(st);
    Py_RETURN_NONE;
}

/* ---- module method table ---- */
static PyMethodDef methods[] = {
    {"open_raw", py_open_raw, METH_VARARGS, "Open a VFDB file and return a DB capsule."},
    {"close_raw", py_close_raw, METH_VARARGS, "Close a VFDB DB capsule."},
    {"prepare", py_prepare, METH_VARARGS, "Prepare SQL and return a statement capsule."},
    {"step", py_step, METH_VARARGS, "Step a statement; returns (True,row) or False when done."},
    {"finalize", py_finalize, METH_VARARGS, "Finalize a statement."},
    {"py_create_function", py_py_create_function, METH_VARARGS, "Register a Python UDF in the DB."},
    {NULL, NULL, 0, NULL}};

/* ---- module definition ---- */
static struct PyModuleDef mod = {
    PyModuleDef_HEAD_INIT,
    "_native",
    NULL,
    -1,
    methods};

PyMODINIT_FUNC PyInit__native(void)
{
    return PyModule_Create(&mod);
}
