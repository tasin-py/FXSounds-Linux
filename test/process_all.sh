#!/bin/sh
# Process test/test_input.wav with power off and with every factory preset.
#   BIN=build-san/fxprocess sh test/process_all.sh
cd "$(dirname "$0")/.."
BIN="${BIN:-build/fxprocess}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
mkdir -p test/out
echo "== bypass (power off)"
"./$BIN" test/test_input.wav test/out/off.wav --off 2>&1 | grep -E 'in :|out:|!!|ERROR|error'
echo "== effects only, no preset (all sliders 5)"
"./$BIN" test/test_input.wav test/out/sliders5.wav --fidelity 5 --ambience 5 --surround 5 --dynamic 5 --bass 5 2>&1 | grep -E 'out:|!!|ERROR|error'
for p in upstream/presets/*.fac; do
  n=$(basename "$p" .fac)
  name=$("./$BIN" --info --preset "$p" 2>/dev/null | sed -n 's/^preset: //p')
  printf '== %-20s ' "$name"
  "./$BIN" test/test_input.wav "test/out/preset_$n.wav" --preset "$p" 2>&1 | grep -E 'out:|!!|ERROR|error' | tr '\n' ' '
  echo
done
