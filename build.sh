#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ACTION="${1:-all}"
VENV="${VFDB_LINUX_VENV:-$HOME/.cache/vfdb/venv}"

ensure_venv() {
  if [[ ! -x "$VENV/bin/python" ]]; then
    if ! python3 -m venv "$VENV" >/dev/null 2>&1; then
      echo "Could not create $VENV."
      echo "On Ubuntu/WSL, install prerequisites with:"
      echo "  sudo apt update && sudo apt install -y python3 python3-venv python3-dev build-essential"
      exit 1
    fi
  fi
  PY="$VENV/bin/python"
}

clean() {
  rm -rf \
    "$ROOT/build" \
    "$ROOT/dist" \
    "$ROOT/.pytest_cache" \
    "$ROOT/vfdb.egg-info" \
    "$ROOT/python/dist" \
    "$ROOT/python/test" \
    "$ROOT/tests/__pycache__" \
    "$ROOT/python/__pycache__" \
    "$ROOT/python/vfdb/__pycache__"
  rm -f "$ROOT"/python/vfdb/_native*.so "$ROOT"/python/vfdb/_native*.pyd
  rm -f "$ROOT"/python/vfdb/_native*.lib "$ROOT"/python/vfdb/_native*.exp "$ROOT"/python/vfdb/vfdb_core.lib
  rm -f "$ROOT"/*.vfdb "$ROOT"/*.vfdb.meta "$ROOT"/*.vfheap
  rm -f "$ROOT"/temp/*.vfdb "$ROOT"/temp/*.vfdb.meta "$ROOT"/temp/*.vfheap 2>/dev/null || true
}

deps() {
  ensure_venv
  "$PY" -m pip install --upgrade pip setuptools wheel pytest build
}

build_ext() {
  ensure_venv
  "$PY" setup.py build_ext
  native="$(find "$ROOT/build" -path '*/vfdb/_native*.so' -type f | head -n 1)"
  if [[ -z "$native" ]]; then
    echo "Build completed but no Linux _native*.so was found under build/."
    exit 1
  fi
  cp -f "$native" "$ROOT/python/vfdb/"
}

test_suite() {
  ensure_venv
  "$PY" -c "import pytest" >/dev/null 2>&1 || "$PY" -m pip install pytest
  "$PY" -m pytest -q
}

smoke() {
  ensure_venv
  "$PY" python/full_db_test.py
}

wheel() {
  ensure_venv
  "$PY" -c "import build" >/dev/null 2>&1 || "$PY" -m pip install build
  "$PY" -m build --wheel
}

cd "$ROOT"
echo "VFDB $ACTION"
echo "Root: $ROOT"
echo "Venv: $VENV"

case "$ACTION" in
  deps) deps ;;
  build) build_ext ;;
  test) test_suite ;;
  smoke) smoke ;;
  wheel) wheel ;;
  clean) clean ;;
  all)
    build_ext
    test_suite
    smoke
    ;;
  *)
    echo "Usage: ./build.sh [deps|build|test|smoke|wheel|clean|all]"
    exit 2
    ;;
esac

echo "VFDB $ACTION complete."
