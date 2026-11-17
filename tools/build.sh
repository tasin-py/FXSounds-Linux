#!/bin/sh
# Full build: DSP + engine (make) and the GUI (CMake/JUCE). Run on Linux from the project root.
#   sh tools/build.sh [Release|Debug]
set -eu
cd "$(dirname "$0")/.."
TYPE="${1:-Release}"
python3 tools/port_gui.py 2>/dev/null || python tools/port_gui.py
make -j"$(nproc)" build/libfxdsp.a build/libfxengine.a build/fxsound-engine
cmake -S . -B build-gui -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" >/dev/null
ninja -C build-gui FxSound
