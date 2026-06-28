#!/bin/bash
# Automated checks for the Luxom ESPHome configs (no hardware needed).
#
#   ./test.sh             # fast: unit tests + validate both YAMLs (esphome config)
#   ./test.sh --compile   # also compile the firmware (catches lambda C++ errors)
#   ./test.sh --coverage  # also measure coverage of the pure logic (luxom_proto.h)
#
# Flags can be combined. If no secrets.yaml is present, the fake
# tests/ci_secrets.yaml is used and removed afterwards (a real one is never touched).
set -euo pipefail
cd "$(dirname "$0")"

YAMLS=(luxom_gateway.yaml luxom_cover_discovery.yaml)

WANT_COMPILE=0; WANT_COV=0
for a in "$@"; do
  [ "$a" = "--compile" ] && WANT_COMPILE=1
  [ "$a" = "--coverage" ] && WANT_COV=1
done

# Pick the ESPHome runner: prefer the uv-pinned one, fall back to a global install.
if command -v uv >/dev/null 2>&1; then
  uv sync --quiet
  ESPHOME="uv run esphome"
elif command -v esphome >/dev/null 2>&1; then
  ESPHOME="esphome"
else
  echo "ERROR: neither uv nor esphome found."; exit 127
fi

# Provide secrets if missing.
CLEANUP=0
if [ ! -f secrets.yaml ]; then
  cp tests/ci_secrets.yaml secrets.yaml
  CLEANUP=1
fi
cleanup() { [ "$CLEANUP" = 1 ] && rm -f secrets.yaml; }
trap cleanup EXIT

rc=0

echo "== unit tests (host, pure logic in luxom_proto.h) =="
if command -v c++ >/dev/null 2>&1; then
  c++ -std=c++17 -Wall tests/test_luxom_proto.cpp -o /tmp/luxom_tests && /tmp/luxom_tests || rc=1
else
  echo "  (skipped: no c++ compiler found)"
fi

if [ "$WANT_COV" = 1 ]; then
  echo "== coverage (host, luxom_proto.h) =="
  CXX="${CXX:-c++}"
  rm -f *.gcda *.gcno *.gcov 2>/dev/null || true
  $CXX -std=c++17 -O0 --coverage tests/test_luxom_proto.cpp -o /tmp/luxom_cov && /tmp/luxom_cov || rc=1
  if command -v gcov >/dev/null 2>&1; then
    gcov -r tests/test_luxom_proto.cpp >/dev/null 2>&1 || true
    [ -f luxom_proto.h.gcov ] && echo "  report: luxom_proto.h.gcov (also uploaded to Codecov in CI)"
  else
    echo "  (gcov not found; raw .gcda/.gcno generated)"
  fi
fi

echo "== validate =="
for y in "${YAMLS[@]}"; do
  echo "  - $y"
  $ESPHOME config "$y" >/dev/null || { echo "    FAILED"; rc=1; }
done

if [ "$WANT_COMPILE" = 1 ]; then
  echo "== compile =="
  for y in "${YAMLS[@]}"; do
    echo "  - $y"
    $ESPHOME compile "$y" || { echo "    FAILED"; rc=1; }
  done
fi

[ "$rc" = 0 ] && echo "OK: all checks passed" || echo "FAILED: see output above"
exit $rc
