#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WOKWI="$ROOT/wokwi"
CLI="${ARDUINO_CLI:-arduino-cli}"

if ! command -v "$CLI" >/dev/null 2>&1; then
  echo "arduino-cli not found. Install it or set ARDUINO_CLI to its path." >&2
  exit 1
fi

echo "Installing ESP32 core (if needed)..."
"$CLI" core update-index
"$CLI" core install esp32:esp32

echo "Installing libraries (if needed)..."
while IFS= read -r lib || [[ -n "$lib" ]]; do
  lib="$(echo "$lib" | sed 's/[[:space:]]//g')"
  [[ -z "$lib" || "$lib" =~ ^# ]] && continue
  "$CLI" lib install "$lib"
done < "$WOKWI/libraries.txt"

mkdir -p "$WOKWI/build"
echo "Compiling wokwi.ino..."
"$CLI" compile \
  --fqbn esp32:esp32:esp32 \
  --output-dir "$WOKWI/build" \
  "$WOKWI/wokwi.ino"

if [[ ! -f "$WOKWI/build/wokwi.ino.bin" ]]; then
  echo "Build finished but firmware binary is missing." >&2
  exit 1
fi

echo "Firmware ready: $WOKWI/build/wokwi.ino.bin"
