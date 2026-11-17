#!/bin/sh
# Builds FxSound-<ver>-x86_64.AppImage from the release tree made by tools/build-release.sh.
#
#   sh tools/build-appimage.sh [OUTDIR]
#
# Needs appimagetool (https://github.com/AppImage/appimagetool/releases, "appimagetool-x86_64.AppImage")
# in PATH or in $APPIMAGETOOL. Not downloaded automatically.
# Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
set -eu
SRC="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mkdir -p "${1:-$SRC/dist}" && cd "${1:-$SRC/dist}" && pwd)"
WORK="${WORK:-$HOME/fxsound-release}"
VER="$(sed -n 's/^set(FX_VERSION "\([0-9.]*\)").*/\1/p' "$SRC/CMakeLists.txt")"
TREE="$WORK/stage/FxSound-$VER-linux-x86_64"
TOOL="${APPIMAGETOOL:-$(command -v appimagetool || command -v appimagetool-x86_64.AppImage || true)}"
[ -d "$TREE" ] || { echo "run tools/build-release.sh first"; exit 1; }
[ -n "$TOOL" ] || { echo "appimagetool not found (set APPIMAGETOOL=/path/to/appimagetool-x86_64.AppImage)"; exit 1; }

APPDIR="$WORK/FxSound.AppDir"
rm -rf "$APPDIR"; mkdir -p "$APPDIR/usr"
cp -a "$TREE/bin" "$TREE/share" "$APPDIR/usr/"
cp "$TREE/LICENSE" "$TREE/README.md" "$APPDIR/usr/share/fxsound/"
sed 's|@EXEC@|FxSound|' "$SRC/packaging/fxsound.desktop" > "$APPDIR/fxsound.desktop"
cp "$SRC/linux/icons/fxsound-256.png" "$APPDIR/fxsound.png"
cp "$SRC/linux/icons/fxsound-256.png" "$APPDIR/.DirIcon"
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
# FxSound finds its presets and fonts in ../share/fxsound next to the binary.
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/FxSound" "$@"
EOF
chmod 755 "$APPDIR/AppRun"
# PipeWire, X11 and freetype come from the system on purpose: libpipewire must match the running daemon.
ARCH=x86_64 "$TOOL" --no-appstream "$APPDIR" "$OUT/FxSound-$VER-x86_64.AppImage"
echo "== $OUT/FxSound-$VER-x86_64.AppImage"
