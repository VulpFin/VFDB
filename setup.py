from __future__ import annotations

import sys
from pathlib import Path

from setuptools import Extension, setup


ROOT = Path(__file__).resolve().parent

CORE_SOURCES = [
    "src/bptree.c",
    "src/catalog.c",
    "src/exec.c",
    "src/fileio.c",
    "src/heap.c",
    "src/parser.c",
    "src/tx.c",
    "src/udf_python.c",
    "src/util.c",
    "src/vf_type.c",
    "src/vfdb.c",
    "src/vfdb_log.c",
]

extra_compile_args: list[str] = []
define_macros: list[tuple[str, str]] = []

if sys.platform == "win32":
    extra_compile_args += ["/O2", "/DNOMINMAX", "/utf-8"]
    define_macros += [("_CRT_SECURE_NO_WARNINGS", "1")]
else:
    extra_compile_args += ["-O3", "-fPIC"]

ext = Extension(
    "vfdb._native",
    sources=["python/vfdb/_native.c", *CORE_SOURCES],
    include_dirs=["include", "src"],
    extra_compile_args=extra_compile_args,
    define_macros=define_macros,
)

setup(
    name="vfdb",
    version="0.1.0",
    description="VFDB: embedded DB with CPython extension",
    long_description=(ROOT / "README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    author="TG11",
    license="AGPL-3.0-or-later",
    python_requires=">=3.10",
    packages=["vfdb"],
    package_dir={"": "python"},
    ext_modules=[ext],
    include_package_data=True,
)
