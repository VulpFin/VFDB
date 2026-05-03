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

# traceutil.py
"""
Tiny trace/debug utility for VFDB test scripts.
Provides: trace(), trace_exc(), trace_section()
"""

import sys
import time
import traceback
from datetime import datetime
from contextlib import contextmanager

# ANSI colors for pretty logs
COLOR_RESET = "\033[0m"
COLOR_BLUE = "\033[94m"
COLOR_GREEN = "\033[92m"
COLOR_YELLOW = "\033[93m"
COLOR_RED = "\033[91m"
USE_COLOR = sys.stdout.isatty()


def trace(msg: str = "", *args, color=COLOR_BLUE, **kwargs):
    """Simple timestamped trace line."""
    t = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    frame = sys._getframe(1)
    prefix = f"[TRACE {frame.f_code.co_filename}:{frame.f_lineno}]"
    text = msg.format(*args, **kwargs)
    if USE_COLOR:
        print(f"{color}{t} {prefix} {text}{COLOR_RESET}", flush=True)
    else:
        print(f"{t} {prefix} {text}", flush=True)


def trace_exc(prefix="Exception caught", color=COLOR_RED):
    """Print current exception with traceback."""
    exc_type, exc_value, tb = sys.exc_info()
    if not exc_type:
        return
    tb_str = "".join(traceback.format_exception(exc_type, exc_value, tb))
    if USE_COLOR:
        print(f"{color}{prefix}:\n{tb_str}{COLOR_RESET}", file=sys.stderr)
    else:
        print(f"{prefix}:\n{tb_str}", file=sys.stderr)


@contextmanager
def trace_section(name: str):
    """Context manager that times a code block."""
    trace(f"BEGIN {name}", color=COLOR_YELLOW)
    start = time.perf_counter()
    try:
        yield
    except Exception:
        trace_exc(f"ERROR in {name}")
        raise
    finally:
        dur = (time.perf_counter() - start) * 1000
        trace(f"END {name} ({dur:.2f} ms)", color=COLOR_GREEN)
