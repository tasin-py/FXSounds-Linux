#!/bin/sh
# Print the DSP state after loading each factory preset.
#   BIN=build-san/fxprocess sh test/presets_info.sh [--brief]
cd "$(dirname "$0")/.."
BIN="${BIN:-build/fxprocess}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
filter() { if [ "${1:-}" = "--brief" ]; then grep -vE '^  band|^EQ bands'; else cat; fi; }
echo "== default state"
"./$BIN" --info 2>&1 | filter "${1:-}"
for p in upstream/presets/*.fac; do
  echo "== $p"
  "./$BIN" --info --preset "$p" 2>&1 | filter "${1:-}"
done
