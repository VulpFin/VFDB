// Copyright (C) 2025 TG11
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
// 
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include "vfdb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Convert textual SQL/VFQL type name → enum */
vf_type vf_type_from_str(const char *s);

/* Canonical enum → short string name (e.g. VF_T_INT -> "INT") */
const char *vf_type_to_str(vf_type t);

int vf_type_uses_int(vf_type t);
int vf_type_uses_real(vf_type t);
int vf_type_uses_text(vf_type t);
int vf_type_uses_blob(vf_type t);
int vf_type_is_numeric(vf_type t);

#ifdef __cplusplus
}
#endif
