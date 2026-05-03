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

cmake_minimum_required(VERSION 3.18)
project(vfdb LANGUAGES C)

# If you already have options/flags, keep them; just ensure -fPIC for Linux
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find Python for building an extension
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

# Sources
set(VFDB_CORE_SOURCES
    src/bptree.c
    src/catalog.c
    src/exec.c
    src/fileio.c
    src/heap.c
    src/parser.c
    src/tx.c
    src/udf_python.c
    src/util.c
    src/vf_type.c
    src/vfdb_log.c
    src/vfdb.c
)

# Build the Python extension module: package.module name
python_add_library(vfdb._native MODULE
    python/vfdb/_native.c
    ${VFDB_CORE_SOURCES}
    WITH_SOABI           # ensure the correct Python ABI tag
)

target_include_directories(vfdb._native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# If you need extra libs (e.g., m for math), link them:
# target_link_libraries(vfdb._native PRIVATE m)

# The filename should be "_native.*.so", not "vfdb._native.*"
set_target_properties(vfdb._native PROPERTIES OUTPUT_NAME "_native")

# Install into the 'vfdb' package inside the wheel
install(TARGETS vfdb._native LIBRARY DESTINATION vfdb)

option(VFDB_DEBUG "Enable chatty logging by default" ON)
if (VFDB_DEBUG)
  target_compile_definitions(vfdb._native PRIVATE VFDB_DEBUG)
endif()
