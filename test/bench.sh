#!/bin/sh
# Measure processing speed of the release build (12 s of 48 kHz stereo audio).
cd "$(dirname "$0")/.."
s=$(date +%s.%N)
for i in 1 2 3 4 5; do ./build/fxprocess test/test_input.wav /tmp/bench.wav --preset upstream/presets/6.fac > /dev/null; done
e=$(date +%s.%N)
echo "5 x 12 s audio in $(awk "BEGIN{printf \"%.2f\", $e - $s}") s  (Classic Processing preset, all effects on)"
cmp -s test/out/preset_6.wav /tmp/bench.wav && echo "release output == ASan build output (bit-exact)" || echo "release output differs from ASan build output"
