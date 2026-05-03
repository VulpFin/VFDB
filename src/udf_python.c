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

/* src/udf_python.c (minimal skeleton) */
#include <Python.h>
#include "vfdb.h"
#include "vfdb_log.h"

/* One interpreter per process; module holds a registry dict */
static int py_ready = 0;
static PyObject *udf_registry = NULL;

static int py_init_once(void)
{
    vfdb_log_init_once();
    LOG_DEBUG("py init once");
    if (py_ready)
        return 1;
    Py_Initialize();
    if (!Py_IsInitialized())
        return 0;
    udf_registry = PyDict_New();
    if (!udf_registry)
        return 0;
    py_ready = 1;
    return 1;
}

vfdb_rc vfdb_py_create_function(VFDB *db, const char *name,
                                const char *args_sig, const char *ret_sig, const char *py_src)
{
    vfdb_log_init_once();
    LOG_DEBUG("vfdb py create function db %p", (void*)db);
    LOG_DEBUG("vfdb py create function name %s ", name);
    LOG_DEBUG("vfdb py create function args sig %s ", args_sig);
    LOG_DEBUG("vfdb py create function ret sig %s ", ret_sig);
    LOG_DEBUG("vfdb py create function py src %s ", py_src);
    (void)db;
    (void)args_sig;
    (void)ret_sig; /* MVP: trust signatures */
    if (!py_init_once())
        return VFDB_ERR;

    /* Compile & extract a callable from provided source */
    PyObject *globals = PyDict_New();
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
    PyObject *code = Py_CompileString(py_src, "<vfdb_udf>", Py_file_input);
    if (!code)
    {
        PyErr_Clear();
        Py_XDECREF(globals);
        return VFDB_ERR;
    }
    PyObject *mod = PyImport_ExecCodeModule("vfdb_udf_tmp", code);
    Py_DECREF(code);
    if (!mod)
    {
        PyErr_Clear();
        Py_XDECREF(globals);
        return VFDB_ERR;
    }

    PyObject *func = PyObject_GetAttrString(mod, name);
    Py_DECREF(mod);
    if (!func || !PyCallable_Check(func))
    {
        Py_XDECREF(func);
        return VFDB_ERR;
    }

    /* Store in registry under its name */
    if (PyDict_SetItemString(udf_registry, name, func) != 0)
    {
        Py_DECREF(func);
        return VFDB_ERR;
    }
    Py_DECREF(func);
    Py_DECREF(globals);
    return VFDB_OK;
}

/* Called by executor node when evaluating FuncCall('py_func', args...) */
PyObject *_vfdb_py_call(const char *name, PyObject *args_tuple)
{
    vfdb_log_init_once();
    LOG_DEBUG("*_vfdb_py_call name %s ", name);
    LOG_DEBUG("*_vfdb_py_call args tuple %p", (void*)args_tuple);
    if (!py_ready)
        return NULL;
    PyObject *func = PyDict_GetItemString(udf_registry, name);
    if (!func)
        return NULL;
    return PyObject_CallObject(func, args_tuple); /* new ref or NULL */
}
