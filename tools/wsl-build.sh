#!/bin/sh
# Build inside WSL on a native (case-sensitive) Linux filesystem.
# Usage (from Windows):  wsl -d archlinux -- sh "/mnt/f/Claude Devs/FXSounds Linux/tools/wsl-build.sh" [make args]
set -eu
SRC="$(cd "$(dirname "$0")/.." && pwd)"
DST="$HOME/fxsound-linux"
mkdir -p "$DST"
# cp -a keeps mtimes, so make only rebuilds what changed
for d in Makefile CMakeLists.txt compat tools upstream engine linux; do cp -a "$SRC/$d" "$DST/"; done
[ -d "$SRC/test" ] && cp -a "$SRC/test" "$DST/"
cd "$DST"
make "$@"
