#!/bin/sh
# FxSound for Linux - installer.
#
#   ./install.sh             install for the current user (no root needed): ~/.local/opt/fxsound
#   sudo ./install.sh --system   install for all users: /opt/fxsound
#   ./install.sh --uninstall     remove it again (add --system for a system install)
#
# The installer only touches its own files: the install folder, one launcher link, one menu entry
# and the FxSound icons. Your settings and presets (~/.config/FxSound) are kept unless you ask.
#
# Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
MODE=user
ACTION=install
YES=0
for a in "$@"; do
    case "$a" in
        --system) MODE=system ;;
        --user) MODE=user ;;
        --uninstall|uninstall) ACTION=uninstall ;;
        -y|--yes) YES=1 ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $a (try --help)"; exit 2 ;;
    esac
done

if [ -t 1 ]; then B="$(printf '\033[1m')"; R="$(printf '\033[31m')"; G="$(printf '\033[32m')"; Y="$(printf '\033[33m')"; N="$(printf '\033[0m')"; else B=; R=; G=; Y=; N=; fi
say()  { printf '%s\n' "$*"; }
ok()   { printf '%s✓%s %s\n' "$G" "$N" "$*"; }
warn() { printf '%s!%s %s\n' "$Y" "$N" "$*"; }
die()  { printf '%s✗ %s%s\n' "$R" "$*" "$N" >&2; exit 1; }

if [ "$MODE" = system ]; then
    [ "$(id -u)" = 0 ] || die "--system needs root: run  sudo $0 --system"
    PREFIX=/opt/fxsound
    BINDIR=/usr/local/bin
    DATA=/usr/local/share
else
    [ "$(id -u)" != 0 ] || die "Run the installer as your normal user (it installs into your home folder). For an install for all users: sudo $0 --system"
    PREFIX="$HOME/.local/opt/fxsound"
    BINDIR="$HOME/.local/bin"
    DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
fi
APPS="$DATA/applications"
ICONS="$DATA/icons/hicolor"

refresh_menus() {
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$APPS" 2>/dev/null || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t "$ICONS" 2>/dev/null || true
    for k in kbuildsycoca6 kbuildsycoca5; do
        if command -v $k >/dev/null 2>&1; then $k >/dev/null 2>&1 || true; break; fi
    done
}

stop_running() {
    # SIGTERM makes FxSound quit normally: it saves your preset and gives the system its
    # normal output device back. A user install only touches this user's FxSound.
    if [ "$MODE" = user ]; then OWN="-u $(id -u)"; else OWN=; fi
    if pgrep $OWN -x FxSound >/dev/null 2>&1; then
        say "Closing the running FxSound..."
        pkill $OWN -TERM -x FxSound 2>/dev/null || true
        i=0
        while pgrep $OWN -x FxSound >/dev/null 2>&1 && [ $i -lt 50 ]; do sleep 0.1; i=$((i+1)); done
        pgrep $OWN -x FxSound >/dev/null 2>&1 && { pkill $OWN -KILL -x FxSound 2>/dev/null || true; }
        WAS_RUNNING=1
    fi
}
WAS_RUNNING=0

# ------------------------------------------------------------------------------- uninstall
if [ "$ACTION" = uninstall ]; then
    stop_running
    [ -d "$PREFIX" ] || [ -e "$BINDIR/fxsound" ] || [ -e "$APPS/fxsound.desktop" ] || { say "FxSound is not installed in $PREFIX."; exit 0; }
    if [ -L "$BINDIR/fxsound" ] || grep -q "FxSound launcher" "$BINDIR/fxsound" 2>/dev/null; then rm -f "$BINDIR/fxsound"; fi
    rm -f "$APPS/fxsound.desktop"
    for s in 16 22 24 32 48 64 128 256; do rm -f "$ICONS/${s}x${s}/apps/fxsound.png"; done
    case "$PREFIX" in */opt/fxsound) rm -rf "$PREFIX" ;; esac
    if [ "$MODE" = user ]; then
        rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/fxsound.desktop"
    fi
    refresh_menus
    ok "FxSound was removed."
    CFG="${XDG_CONFIG_HOME:-$HOME/.config}/FxSound"
    if [ "$MODE" = user ] && [ -d "$CFG" ]; then
        if [ $YES = 0 ] && [ -t 0 ]; then
            printf 'Also delete your FxSound settings and saved presets (%s)? [y/N] ' "$CFG"
            read -r ans || ans=
            case "$ans" in y|Y|yes|YES) rm -rf "$CFG"; ok "Settings deleted." ;; *) say "Your settings and presets were kept in $CFG" ;; esac
        else
            say "Your settings and presets were kept in $CFG"
        fi
    fi
    exit 0
fi

# ------------------------------------------------------------------------------- checks
say "${B}FxSound for Linux${N} - $(cat "$HERE/VERSION" 2>/dev/null || echo '?') - installing for $( [ "$MODE" = system ] && echo 'all users' || echo "$(id -un)" )"
[ -x "$HERE/bin/FxSound" ] || die "bin/FxSound is missing - please extract the whole archive and run install.sh from inside it."

case "$(uname -m)" in x86_64|amd64) ;; *) die "This build is for 64-bit Intel/AMD PCs (x86_64); this machine is $(uname -m)." ;; esac

pm_hint() {
    if command -v apt-get >/dev/null 2>&1; then echo "sudo apt install $1"
    elif command -v dnf >/dev/null 2>&1; then echo "sudo dnf install $2"
    elif command -v pacman >/dev/null 2>&1; then echo "sudo pacman -S $3"
    elif command -v zypper >/dev/null 2>&1; then echo "sudo zypper install $4"
    else echo "install: $1"; fi
}

missing="$(ldd "$HERE/bin/FxSound" 2>/dev/null | awk '/not found/{print $1}' | tr '\n' ' ')"
if ldd "$HERE/bin/FxSound" 2>&1 | grep -q "GLIBC_.*not found"; then
    die "Your Linux is too old for this build (it needs glibc 2.39 or newer, e.g. Ubuntu 24.04, Debian 13, Fedora 40, or any current rolling distro)."
fi
if [ -n "$missing" ]; then
    case "$missing" in *pipewire*)
        die "PipeWire is not installed. FxSound needs the PipeWire sound system: $(pm_hint libpipewire-0.3-0t64 pipewire-libs pipewire libpipewire-0_3-0)" ;;
    esac
    die "Missing libraries: $missing
  Install them with your package manager, e.g.: $(pm_hint 'libx11-6 libfreetype6 libfontconfig1' 'libX11 freetype fontconfig' 'libx11 freetype2 fontconfig' 'libX11-6 libfreetype6 fontconfig')"
fi
ok "All needed libraries are present"

# tray icon: KDE Plasma and GNOME (Wayland) only show StatusNotifier icons, which need this library
if ! { ldconfig -p 2>/dev/null | grep -q -e 'libayatana-appindicator3\.so\.1' -e 'libappindicator3\.so\.1'; } \
   && ! ls /usr/lib/libayatana-appindicator3.so.1 /usr/lib64/libayatana-appindicator3.so.1 /usr/lib/x86_64-linux-gnu/libayatana-appindicator3.so.1 >/dev/null 2>&1; then
    warn "For the FxSound tray icon, please also install the AppIndicator library:"
    warn "  $(pm_hint libayatana-appindicator3-1 libayatana-appindicator-gtk3 libayatana-appindicator libayatana-appindicator3-1)"
fi

if [ "$MODE" = user ]; then
    rt="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
    if [ -S "$rt/pipewire-0" ]; then
        ok "PipeWire sound server is running"
    else
        warn "The PipeWire sound server does not seem to be running in this session."
        warn "FxSound works with PipeWire (the default on current Fedora, Ubuntu, Debian, Arch, openSUSE, Kali...)."
        warn "If your system still uses PulseAudio, switch to PipeWire first (pipewire-pulse + wireplumber)."
    fi
fi

# ------------------------------------------------------------------------------- install
stop_running
STAGE="$PREFIX.new.$$"
rm -rf "$STAGE"
mkdir -p "$STAGE" || die "Cannot write to $(dirname "$PREFIX")"
cp -R "$HERE/bin" "$HERE/share" "$STAGE/"
for f in VERSION LICENSE README.md install.sh; do [ -f "$HERE/$f" ] && cp "$HERE/$f" "$STAGE/"; done
chmod 755 "$STAGE/bin/FxSound" "$STAGE/install.sh"
# keep any earlier copy until the new one is completely in place
if [ -d "$PREFIX" ]; then rm -rf "$PREFIX.old"; mv "$PREFIX" "$PREFIX.old"; fi
mv "$STAGE" "$PREFIX"
rm -rf "$PREFIX.old"
ok "Installed to $PREFIX"

mkdir -p "$BINDIR"
if [ -e "$BINDIR/fxsound" ] && [ ! -L "$BINDIR/fxsound" ] && ! grep -q "FxSound launcher" "$BINDIR/fxsound" 2>/dev/null; then
    warn "$BINDIR/fxsound exists and is not ours - left it alone."
else
    # a wrapper, not a symlink: the process must be named "FxSound" (a symlink would make it "fxsound")
    rm -f "$BINDIR/fxsound"
    printf '#!/bin/sh
# FxSound launcher
exec "%s/bin/FxSound" "$@"
' "$PREFIX" > "$BINDIR/fxsound"
    chmod 755 "$BINDIR/fxsound"
fi

mkdir -p "$APPS"
sed "s|@EXEC@|\"$PREFIX/bin/FxSound\"|" "$HERE/share/fxsound/fxsound.desktop" > "$APPS/fxsound.desktop"
chmod 644 "$APPS/fxsound.desktop"
for s in 16 22 24 32 48 64 128 256; do
    if [ -f "$HERE/share/icons/fxsound-$s.png" ]; then
        mkdir -p "$ICONS/${s}x${s}/apps"
        cp "$HERE/share/icons/fxsound-$s.png" "$ICONS/${s}x${s}/apps/fxsound.png"
    fi
done
refresh_menus
ok "Added FxSound to your applications menu"

# an existing "Launch on system startup" entry must point at the new location
AUTO="${XDG_CONFIG_HOME:-$HOME/.config}/autostart/fxsound.desktop"
if [ "$MODE" = user ] && [ -f "$AUTO" ] && ! grep -q "Hidden=true" "$AUTO"; then
    sed -i "s|^Exec=.*|Exec=\"$PREFIX/bin/FxSound\" --run_minimized|" "$AUTO"
fi

say ""
say "${B}Done!${N} Start FxSound from your applications menu$( [ "$MODE" = user ] && echo ", or run: $PREFIX/bin/FxSound" )."
say "To remove it later:  $PREFIX/install.sh --uninstall$( [ "$MODE" = system ] && echo ' --system' )"

if [ "$MODE" = user ] && [ $YES = 0 ] && [ -t 0 ] && { [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; }; then
    if [ $WAS_RUNNING = 1 ]; then ans=y; else printf 'Start FxSound now? [Y/n] '; read -r ans || ans=n; fi
    case "$ans" in n|N|no|NO) ;; *) (cd / && setsid "$PREFIX/bin/FxSound" >/dev/null 2>&1 &) ; ok "FxSound started" ;; esac
fi
