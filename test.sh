#!/bin/bash
# Automated checks for the Luxom ESPHome configs (no hardware needed).
#
#   ./test.sh            # fast: validate both YAMLs (esphome config)
#   ./test.sh --compile  # full: also compile the firmware (catches lambda C++ errors)
#
# If no secrets.yaml is present, the fake tests/ci_secrets.yaml is used and removed
# afterwards, so a real secrets.yaml is never touched.
set -euo pipefail
cd "$(dirname "$0")"

YAMLS=(luxom_gateway.yaml luxom_cover_discovery.yaml)

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

echo "== validate =="
for y in "${YAMLS[@]}"; do
  echo "  - $y"
  $ESPHOME config "$y" >/dev/null || { echo "    FAILED"; rc=1; }
done

if [ "${1:-}" = "--compile" ]; then
  echo "== compile =="
  for y in "${YAMLS[@]}"; do
    echo "  - $y"
    $ESPHOME compile "$y" || { echo "    FAILED"; rc=1; }
  done
fi

[ "$rc" = 0 ] && echo "OK: all checks passed" || echo "FAILED: see output above"
exit $rc
