#!/bin/sh
# Re-run the "sliders at 5, no preset" case with the Windows registry session imported.
cd "$(dirname "$0")/.."
BIN="${BIN:-build/fxprocess}"
FX_DEBUG=1 FX_REGISTRY_IMPORT=test/fixtures/windows_session.tsv "./$BIN" test/test_input.wav test/out/sliders5.wav \
  --fidelity 5 --ambience 5 --surround 5 --dynamic 5 --bass 5 2>&1 | grep -E 'imported|out:'
