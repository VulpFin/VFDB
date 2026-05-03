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

#ifndef VFDB_COMPAT_H
#define VFDB_COMPAT_H

#include <string.h>
#include <time.h>

#ifdef _WIN32
  /* Quiet MSVC's "secure CRT" noise and avoid <windows.h> min/max traps */
  #ifndef _CRT_SECURE_NO_WARNINGS
  #define _CRT_SECURE_NO_WARNINGS 1
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX 1
  #endif

  /* Windows CRT equivalents */
  #define vfdb_strdup    _strdup
  #define vfdb_stricmp   _stricmp
  #define vfdb_strnicmp  _strnicmp

  /* MSVC has snprintf since VS2015; prefer it over _snprintf */
  #define vfdb_snprintf  snprintf

  /* POSIX-y names some code might call directly */
  #ifndef strcasecmp
  #define strcasecmp     _stricmp
  #endif
  #ifndef strncasecmp
  #define strncasecmp    _strnicmp
  #endif

  /* localtime_r shim via localtime_s */
  static inline struct tm* localtime_r(const time_t *src, struct tm *dst) {
      return (localtime_s(dst, src) == 0) ? dst : NULL;
  }

#else
  /* POSIX / glibc */
  #define vfdb_strdup    strdup
  #define vfdb_stricmp   strcasecmp
  #define vfdb_strnicmp  strncasecmp
  #define vfdb_snprintf  snprintf
#endif

#endif /* VFDB_COMPAT_H */

