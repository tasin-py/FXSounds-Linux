#!/bin/sh
# Release build + packages of FxSound for Linux.
#
# Build on the OLDEST distro you want to support (the binary needs that glibc or newer); the
# reference build host is Ubuntu 24.04 (glibc 2.39, PipeWire 1.0). Needs: build-essential cmake
# ninja-build pkg-config python3 libpipewire-0.3-dev libx11-dev libxrandr-dev libxinerama-dev
# libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev dpkg-dev, and
# JUCE 7 in $JUCE_DIR (default ~/third_party/JUCE).
#
#   sh tools/build-release.sh [OUTDIR]
#
# Produces in OUTDIR (default ./dist):
#   FxSound-<ver>-linux-x86_64.tar.gz   universal: extract, run ./install.sh
#   fxsound_<ver>_amd64.deb             Debian / Ubuntu / Mint / Pop!_OS / Kali
#   fxsound-<ver>-pkgroot.tar.gz        file tree used by the rpm spec and the Arch PKGBUILD
#
# The GUI sources in upstream/ must already be ported (tools/port_gui.py).
# Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
set -eu
SRC="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mkdir -p "${1:-$SRC/dist}" && cd "${1:-$SRC/dist}" && pwd)"
WORK="${WORK:-$HOME/fxsound-release}"
JUCE_DIR="${JUCE_DIR:-$HOME/third_party/JUCE}"
[ -f "$JUCE_DIR/CMakeLists.txt" ] || { echo "JUCE not found in $JUCE_DIR"; exit 1; }

VER="$(sed -n 's/^set(FX_VERSION "\([0-9.]*\)").*/\1/p' "$SRC/CMakeLists.txt")"
echo "== FxSound for Linux $VER: building in $WORK"
mkdir -p "$WORK"
for d in Makefile CMakeLists.txt compat tools engine linux upstream packaging LICENSE; do cp -a "$SRC/$d" "$WORK/"; done
cd "$WORK"
make -j"$(nproc)" build/libfxdsp.a build/libfxengine.a
cmake -S . -B build-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR="$JUCE_DIR" -DFX_MAKE_BUILD="$WORK/build" >/dev/null
ninja -C build-rel FxSound
BIN=build-rel/FxSound_artefacts/Release/FxSound

# ---------------------------------------------------------------- portable tree
NAME="FxSound-$VER-linux-x86_64"
T="$WORK/stage/$NAME"
rm -rf "$WORK/stage"; mkdir -p "$T/bin" "$T/share/fxsound/Factsoft" "$T/share/icons"
install -m755 "$BIN" "$T/bin/FxSound"
strip --strip-unneeded "$T/bin/FxSound"
install -m644 upstream/presets/*.fac "$T/share/fxsound/Factsoft/"
install -m644 upstream/fxsound/Fonts-runtime/* "$T/share/fxsound/"
install -m644 packaging/fxsound.desktop "$T/share/fxsound/fxsound.desktop"
for s in 16 22 24 32 48 64 128 256; do install -m644 "linux/icons/fxsound-$s.png" "$T/share/icons/"; done
install -m755 packaging/install.sh "$T/install.sh"
install -m644 packaging/README.md "$T/README.md"
install -m644 LICENSE "$T/LICENSE"
echo "$VER" > "$T/VERSION"
tar -C "$WORK/stage" -czf "$OUT/$NAME.tar.gz" "$NAME"
echo "== $OUT/$NAME.tar.gz"

# ---------------------------------------------------------------- package root (/opt/fxsound) shared by deb/rpm/arch
P="$WORK/pkgroot"
rm -rf "$P"; mkdir -p "$P/opt/fxsound" "$P/usr/bin" "$P/usr/share/applications"
cp -a "$T/bin" "$T/share" "$T/VERSION" "$T/LICENSE" "$T/README.md" "$P/opt/fxsound/"
rm -rf "$P/opt/fxsound/share/icons"
# a wrapper, not a symlink: the process must be named "FxSound" (a symlink would make it "fxsound")
printf '#!/bin/sh
exec /opt/fxsound/bin/FxSound "$@"
' > "$P/usr/bin/fxsound"; chmod 755 "$P/usr/bin/fxsound"
sed 's|@EXEC@|/opt/fxsound/bin/FxSound|' packaging/fxsound.desktop > "$P/usr/share/applications/fxsound.desktop"
for s in 16 22 24 32 48 64 128 256; do
    mkdir -p "$P/usr/share/icons/hicolor/${s}x${s}/apps"
    install -m644 "linux/icons/fxsound-$s.png" "$P/usr/share/icons/hicolor/${s}x${s}/apps/fxsound.png"
done
mkdir -p "$P/usr/share/licenses/fxsound"; cp LICENSE "$P/usr/share/licenses/fxsound/"
tar -C "$P" -czf "$OUT/fxsound-$VER-pkgroot.tar.gz" .
echo "== $OUT/fxsound-$VER-pkgroot.tar.gz"

# ---------------------------------------------------------------- .deb
if command -v dpkg-deb >/dev/null 2>&1; then
    D="$WORK/deb"
    rm -rf "$D"; cp -a "$P" "$D"; mkdir -p "$D/DEBIAN"
    SIZE="$(du -sk "$D" | cut -f1)"
    sed -e "s/@VERSION@/$VER/" -e "s/@SIZE@/$SIZE/" packaging/deb/control > "$D/DEBIAN/control"
    install -m755 packaging/deb/prerm "$D/DEBIAN/prerm"
    install -m755 packaging/deb/postinst "$D/DEBIAN/postinst"
    install -m755 packaging/deb/postrm "$D/DEBIAN/postrm"
    dpkg-deb --root-owner-group -Zxz --build "$D" "$OUT/fxsound_${VER}_amd64.deb" >/dev/null
    echo "== $OUT/fxsound_${VER}_amd64.deb"
fi
ls -la "$OUT"
