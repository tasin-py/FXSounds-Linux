#!/bin/sh
# Copy test outputs from the WSL build dir back to the Windows project folder.
SRC="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$SRC/test/out"
cp ~/fxsound-linux/test/out/*.wav "$SRC/test/out/"
ls "$SRC/test/out" | wc -l
