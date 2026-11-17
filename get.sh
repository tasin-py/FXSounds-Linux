#!/bin/sh
# FxSound for Linux - one-line installer.
#
#   curl -fsSL https://raw.githubusercontent.com/tasin-py/FXSounds-Linux/main/get.sh | sh
#
# Downloads the latest release from GitHub and installs it:
#   Debian / Ubuntu / Mint / Kali / Pop!_OS   -> the .deb      (apt, asks for your password)
#   Fedora / openSUSE                         -> the .rpm      (dnf / zypper, asks for your password)
#   Arch / Manjaro / EndeavourOS              -> the package   (pacman, asks for your password)
#   anything else, or with --user             -> just for you, in your home folder (no password)
#
# Options (after "sh -s --" when piping):  --user   install only for you, without root
#                                          --uninstall
# Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
set -eu

REPO="tasin-py/FXSounds-Linux"
MODE=auto
for a in "$@"; do
    case "$a" in
        --user) MODE=user ;;
        --uninstall) MODE=uninstall ;;
        -h|--help) sed -n '2,15p' "$0" 2>/dev/null || true; exit 0 ;;
        *) echo "Unknown option: $a"; exit 2 ;;
    esac
done

if [ -t 1 ]; then B="$(printf '\033[1m')"; R="$(printf '\033[31m')"; G="$(printf '\033[32m')"; N="$(printf '\033[0m')"; else B=; R=; G=; N=; fi
say()  { printf '%s\n' "${B}==>${N} $*"; }
die()  { printf '%s\n' "${R}Error:${N} $*" >&2; exit 1; }

[ "$(uname -s)" = Linux ] || die "FxSound for Linux runs on Linux only."
[ "$(uname -m)" = x86_64 ] || die "FxSound for Linux needs a 64-bit Intel/AMD PC (x86_64); this one is $(uname -m)."
command -v curl >/dev/null 2>&1 || die "curl is needed (install it with your package manager)."
[ "$(id -u)" = 0 ] && [ "$MODE" = user ] && die "--user installs for one user: run it as that user, not as root."

SUDO=""
if [ "$(id -u)" != 0 ]; then
    if command -v sudo >/dev/null 2>&1; then SUDO=sudo
    elif command -v doas >/dev/null 2>&1; then SUDO=doas
    fi
fi

ID=""; ID_LIKE=""
[ -r /etc/os-release ] && . /etc/os-release
FAMILY=""
for x in $ID $ID_LIKE; do
    case "$x" in
        debian|ubuntu) FAMILY=deb; break ;;
        fedora|rhel|centos) FAMILY=rpm-dnf; break ;;
        opensuse*|suse|sles) FAMILY=rpm-zypper; break ;;
        arch|archlinux) FAMILY=arch; break ;;
    esac
done
case "$FAMILY" in
    deb) command -v apt-get >/dev/null 2>&1 || FAMILY="" ;;
    rpm-dnf) command -v dnf >/dev/null 2>&1 || FAMILY="" ;;
    rpm-zypper) command -v zypper >/dev/null 2>&1 || FAMILY="" ;;
    arch) command -v pacman >/dev/null 2>&1 || FAMILY="" ;;
esac

# ---------------------------------------------------------------- uninstall
if [ "$MODE" = uninstall ]; then
    done_any=""
    if [ -x "$HOME/.local/opt/fxsound/install.sh" ]; then "$HOME/.local/opt/fxsound/install.sh" --uninstall -y; done_any=1; fi
    if [ -e /opt/fxsound ]; then
        say "Removing the system-wide FxSound package"
        if command -v dpkg >/dev/null 2>&1 && dpkg -s fxsound >/dev/null 2>&1; then $SUDO apt-get remove -y fxsound
        elif command -v pacman >/dev/null 2>&1 && pacman -Q fxsound-bin >/dev/null 2>&1; then $SUDO pacman -R --noconfirm fxsound-bin
        elif command -v rpm >/dev/null 2>&1 && rpm -q fxsound >/dev/null 2>&1; then
            if command -v dnf >/dev/null 2>&1; then $SUDO dnf remove -y fxsound; else $SUDO zypper -n remove fxsound; fi
        elif [ -x /opt/fxsound/install.sh ]; then $SUDO /opt/fxsound/install.sh --uninstall -y
        fi
        done_any=1
    fi
    [ -n "$done_any" ] && say "${G}FxSound was removed.${N} Your settings and presets stay in ~/.config/FxSound." || say "FxSound is not installed."
    exit 0
fi

# ---------------------------------------------------------------- latest release
say "Looking for the latest FxSound for Linux release"
URL="$(curl -fsSLI -o /dev/null -w '%{url_effective}' "https://github.com/$REPO/releases/latest")" \
    || die "Could not reach GitHub. Check your internet connection."
TAG="${URL##*/releases/tag/}"
case "$TAG" in ""|http*) die "No release found at https://github.com/$REPO/releases" ;; esac
VER="${TAG#v}"
DL="https://github.com/$REPO/releases/download/$TAG"
say "Latest version: $VER"

# the binary needs glibc 2.39 or newer
GLIBC="$(getconf GNU_LIBC_VERSION 2>/dev/null | awk '{print $2}')"
if [ -n "$GLIBC" ]; then
    if [ "$(printf '%s\n2.39\n' "$GLIBC" | sort -V | head -1)" != 2.39 ]; then
        die "Your system is too old for FxSound for Linux (glibc $GLIBC, needs 2.39 or newer:
       Ubuntu 24.04+, Debian 13+, Fedora 40+, Mint 22+, or a current Arch / openSUSE Tumbleweed)."
    fi
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT INT TERM
chmod 755 "$TMP"

fetch() {  # fetch <file>: download + verify against the release's SHA256SUMS
    say "Downloading $1"
    curl -fL --progress-bar -o "$TMP/$1" "$DL/$1" || die "Download failed: $DL/$1"
    chmod 644 "$TMP/$1"
    if [ -z "${SUMS_DONE:-}" ]; then
        curl -fsSL -o "$TMP/SHA256SUMS" "$DL/SHA256SUMS" 2>/dev/null || rm -f "$TMP/SHA256SUMS"
        SUMS_DONE=1
    fi
    if [ -f "$TMP/SHA256SUMS" ] && command -v sha256sum >/dev/null 2>&1; then
        (cd "$TMP" && grep " $1\$" SHA256SUMS | sed 's/ \*/  /' | sha256sum -c --status) \
            || die "The download of $1 is damaged (checksum mismatch). Please try again."
    fi
}

user_install() {
    f="FxSound-$VER-linux-x86_64.tar.gz"
    fetch "$f"
    tar -xzf "$TMP/$f" -C "$TMP"
    "$TMP/FxSound-$VER-linux-x86_64/install.sh" -y
}

if [ "$MODE" = user ] || [ -z "$FAMILY" ] || { [ "$(id -u)" != 0 ] && [ -z "$SUDO" ]; }; then
    [ "$MODE" != user ] && [ -z "$FAMILY" ] && say "Unknown distro (${ID:-?}): installing just for you, in your home folder."
    [ "$MODE" != user ] && [ -n "$FAMILY" ] && say "sudo is not available: installing just for you, in your home folder."
    user_install
    exit 0
fi

# a per-user copy would hide the system-wide one: remove it first
if [ -x "$HOME/.local/opt/fxsound/install.sh" ]; then
    say "Removing your older per-user FxSound (settings and presets are kept)"
    "$HOME/.local/opt/fxsound/install.sh" --uninstall -y >/dev/null 2>&1 || true
fi

[ -n "$SUDO" ] && say "Installing for all users: $SUDO may ask for your password."
case "$FAMILY" in
    deb)
        f="fxsound_${VER}_amd64.deb"; fetch "$f"
        $SUDO apt-get install -y "$TMP/$f" ;;
    rpm-dnf)
        f="fxsound-$VER-1.x86_64.rpm"; fetch "$f"
        $SUDO dnf install -y "$TMP/$f" ;;
    rpm-zypper)
        f="fxsound-$VER-1.x86_64.rpm"; fetch "$f"
        $SUDO zypper -n --no-gpg-checks install --allow-unsigned-rpm "$TMP/$f" ;;
    arch)
        f="fxsound-bin-$VER-1-x86_64.pkg.tar.zst"; fetch "$f"
        $SUDO pacman -U --noconfirm "$TMP/$f" ;;
esac

say "${G}FxSound $VER is installed.${N} Start it from your app menu, or run: fxsound"
