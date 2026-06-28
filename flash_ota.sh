#!/bin/bash
# Build and flash a Luxom gateway config over OTA (or USB on first flash).
#
# Usage:
#   ./flash_ota.sh gateway   [ip|device]   # production gateway (default)
#   ./flash_ota.sh discovery [ip|device]   # PHASE 1 shutter discovery tool
#
# First flash must be over USB: ./flash_ota.sh gateway /dev/ttyUSB0
# Afterwards OTA by IP:         ./flash_ota.sh gateway 192.168.0.60
set -euo pipefail
cd "$(dirname "$0")"

TARGET="${1:-gateway}"
DEVICE="${2:-}"

case "$TARGET" in
  gateway)   YAML="luxom_gateway.yaml" ;;
  discovery) YAML="luxom_cover_discovery.yaml" ;;
  *) echo "Unknown target '$TARGET' (use: gateway | discovery)"; exit 1 ;;
esac

if [ ! -f secrets.yaml ]; then
  echo "ERROR: secrets.yaml not found. Copy secrets.yaml.example to secrets.yaml and fill it in."
  exit 1
fi

echo "=== Syncing uv environment ==="
uv sync

echo "=== ESPHome $(uv run esphome version 2>/dev/null | awk '{print $2}') ==="

if [ -n "$DEVICE" ]; then
  uv run esphome run "$YAML" --device "$DEVICE"
else
  # no device given: ESPHome will pick OTA/serial interactively
  uv run esphome run "$YAML"
fi
