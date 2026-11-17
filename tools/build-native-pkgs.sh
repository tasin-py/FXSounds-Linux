#!/bin/sh
# Builds the .rpm or the Arch package from dist/fxsound-<ver>-pkgroot.tar.gz (made by tools/build-release.sh).
#   sh tools/build-native-pkgs.sh rpm     (as root on Fedora/openSUSE with rpm-build)
#   sh tools/build-native-pkgs.sh arch    (on Arch with base-devel; run as root: uses a "builder" user)
# Then writes dist/SHA256SUMS for all release files.
set -eu
SRC="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$SRC/dist"
VER="$(sed -n 's/^set(FX_VERSION "\([0-9.]*\)").*/\1/p' "$SRC/CMakeLists.txt")"
case "${1:-}" in
    rpm)
        rm -rf /tmp/rpmout
        rpmbuild -bb --define "_sourcedir $DIST" --define "_rpmdir /tmp/rpmout" --define "fxversion $VER" "$SRC/packaging/rpm/fxsound.spec" >/tmp/rpmbuild.log 2>&1 \
            || { tail -20 /tmp/rpmbuild.log; exit 1; }
        cp /tmp/rpmout/x86_64/fxsound-$VER-1.x86_64.rpm "$DIST/"
        echo "== $DIST/fxsound-$VER-1.x86_64.rpm" ;;
    arch)
        B=/tmp/fxsound-pkgbuild
        rm -rf "$B"; mkdir -p "$B"
        cp "$SRC/packaging/arch/PKGBUILD" "$SRC/packaging/arch/fxsound.install" "$DIST/fxsound-$VER-pkgroot.tar.gz" "$B/"
        sed -i "s/^pkgver=.*/pkgver=$VER/; s/^sha256sums=.*/sha256sums=('$(sha256sum "$B/fxsound-$VER-pkgroot.tar.gz" | cut -d' ' -f1)')/" "$B/PKGBUILD"
        id builder >/dev/null 2>&1 || useradd -m builder
        chown -R builder "$B"
        su builder -s /bin/sh -c "cd $B && makepkg -f --nodeps >makepkg.log 2>&1" || { tail -20 "$B/makepkg.log"; exit 1; }
        cp "$B"/fxsound-bin-$VER-1-x86_64.pkg.tar.zst "$DIST/"
        echo "== $DIST/fxsound-bin-$VER-1-x86_64.pkg.tar.zst" ;;
    sums) ;;
    *) echo "usage: $0 rpm|arch|sums"; exit 2 ;;
esac
cd "$DIST"
sha256sum FxSound-$VER-linux-x86_64.tar.gz fxsound_${VER}_amd64.deb fxsound-$VER-1.x86_64.rpm \
    fxsound-bin-$VER-1-x86_64.pkg.tar.zst fxsound-$VER-pkgroot.tar.gz 2>/dev/null > SHA256SUMS || true
echo "== $DIST/SHA256SUMS"
