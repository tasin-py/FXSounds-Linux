#!/bin/sh
# End-to-end test of a FxSound release on one Linux distro (run as root inside the distro, e.g. a WSL
# distro):  sh test/distro_test.sh <label> <display-number> [deb|rpm|arch]
#
#  1. installs PipeWire + test tools from the distro's own repositories
#  2. as a normal user: extracts the release tarball and runs ./install.sh (the real user install)
#  3. starts a headless PipeWire session with a fake speaker, starts the installed FxSound on Xvfb
#  4. checks: engine started, FxSound became the default output, plays test audio through it and
#     records what reaches the speaker (test/out/distro_<label>.wav, compared later bit-for-bit)
#  5. SIGTERM -> the default output goes back to the speaker; uninstall removes everything
#  6. optionally installs the native package (.deb/.rpm/arch) and starts it from /opt/fxsound
set -u
LABEL="$1"; DNUM="$2"; NATIVE="${3:-}"
PROJ="/mnt/f/Claude Devs/FXSounds Linux"
DIST="$PROJ/dist"
OUT="$PROJ/test/out"; SHOTS="$PROJ/test/screens"
VER="$(sed -n 's/^set(FX_VERSION "\([0-9.]*\)").*/\1/p' "$PROJ/CMakeLists.txt")"
TU=fxtest
PASS=0; FAIL=0
pass() { echo "PASS: $*"; PASS=$((PASS+1)); }
fail() { echo "FAIL: $*"; FAIL=$((FAIL+1)); }

echo "=== $LABEL: $(grep PRETTY_NAME /etc/os-release | cut -d= -f2) / $(ldd --version | head -1)"

# ---------------------------------------------------------------- 1. packages
if command -v apt-get >/dev/null; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq >/dev/null 2>&1
    apt-get install -y -qq pipewire pipewire-bin wireplumber xvfb xdotool imagemagick procps sudo dbus >/dev/null 2>&1
elif command -v dnf >/dev/null; then
    dnf install -y -q pipewire pipewire-utils wireplumber xorg-x11-server-Xvfb xdotool ImageMagick procps-ng sudo >/dev/null 2>&1
elif command -v pacman >/dev/null; then
    pacman -Sy --noconfirm --needed pipewire pipewire-audio wireplumber xorg-server-xvfb xdotool imagemagick procps-ng sudo >/dev/null 2>&1
elif command -v zypper >/dev/null; then
    for p in pipewire pipewire-tools wireplumber xorg-x11-server-Xvfb xdotool ImageMagick procps sudo dbus-1; do
        zypper -n -q install $p >/dev/null 2>&1 || echo "(zypper: $p not installed)"
    done
fi
# no Xvfb (openSUSE-WSL: it conflicts with patterns-wsl-tmpfiles) -> use the WSLg display instead
if ! command -v Xvfb >/dev/null && [ -S /tmp/.X11-unix/X0 ]; then DNUM=0; NOXVFB=1; echo "(no Xvfb: using the WSLg display :0)"; fi
for t in pipewire wireplumber pw-record pw-play pw-metadata; do
    command -v $t >/dev/null || { fail "test tool $t missing"; }
done
IMPORT="$(command -v magick >/dev/null && echo 'magick import' || echo import)"

# ---------------------------------------------------------------- 2. user + install
id $TU >/dev/null 2>&1 || useradd -m $TU
UID_T="$(id -u $TU)"
RT="/tmp/fxrt-$UID_T"
pkill -u $TU -x FxSound 2>/dev/null; pkill -u $TU -x pipewire 2>/dev/null; pkill -u $TU -x wireplumber 2>/dev/null; pkill -u $TU -x dbus-daemon 2>/dev/null
[ "$DNUM" != 0 ] && pkill -f "Xvfb :$DNUM" 2>/dev/null
sleep 0.5
rm -rf "$RT" /tmp/fxsound-install; mkdir -p "$RT" /tmp/fxsound-install; chown $TU "$RT" /tmp/fxsound-install; chmod 700 "$RT"
cp "$DIST/FxSound-$VER-linux-x86_64.tar.gz" /tmp/fxsound-install/ && chown $TU /tmp/fxsound-install/*
cp "$PROJ/test/test_input.wav" /tmp/fxsound-install/ && chmod 644 /tmp/fxsound-install/test_input.wav

asuser() { su $TU -s /bin/sh -c "export HOME=/home/$TU XDG_RUNTIME_DIR=$RT DBUS_SESSION_BUS_ADDRESS=\$(cat $RT/bus 2>/dev/null || echo disabled:) DISPLAY=:$DNUM XDG_SESSION_TYPE=x11; unset WAYLAND_DISPLAY; $1"; }

asuser "rm -rf ~/.config/FxSound ~/.local/opt/fxsound ~/.config/autostart/fxsound.desktop"
asuser "cd /tmp/fxsound-install && tar xzf FxSound-$VER-linux-x86_64.tar.gz && cd FxSound-$VER-linux-x86_64 && ./install.sh -y" > /tmp/fxsound-install/install.log 2>&1
sed 's/\x1b\[[0-9;]*m//g' /tmp/fxsound-install/install.log
asuser "test -x ~/.local/opt/fxsound/bin/FxSound && test -f ~/.local/share/applications/fxsound.desktop && test -x ~/.local/bin/fxsound && test -f ~/.local/share/icons/hicolor/256x256/apps/fxsound.png" \
    && pass "user install: files, menu entry, launcher link, icons" || fail "user install files"

# ---------------------------------------------------------------- 3. session
# private session bus (WirePlumber 0.4, e.g. Ubuntu 24.04, exits without one; real desktops always have it)
if command -v dbus-daemon >/dev/null; then
    asuser "dbus-daemon --session --fork --print-address=1 > $RT/bus.tmp 2>/dev/null && head -1 $RT/bus.tmp > $RT/bus"
fi
asuser "setsid -f pipewire >$RT/pw.log 2>&1 </dev/null; sleep 1; setsid -f wireplumber >$RT/wp.log 2>&1 </dev/null; sleep 2
pw-cli create-node adapter '{ factory.name=support.null-audio-sink node.name=FakeSpeakers node.description=\"Fake Speakers\" media.class=Audio/Sink object.linger=true audio.format=F32 audio.position=[FL FR] audio.rate=48000 }' >/dev/null; sleep 1"
[ -z "${NOXVFB:-}" ] && setsid -f Xvfb :$DNUM -screen 0 1600x1000x24 >/tmp/xvfb$DNUM.log 2>&1 </dev/null
sleep 1
PWV="$(asuser 'pipewire --version' 2>/dev/null | grep -o 'libpipewire [0-9.]*')"
echo "PipeWire: $PWV"

defsink() { asuser "pw-metadata 0 default.configured.audio.sink 2>/dev/null" | grep -o '"name": "[^"]*"' | head -1; }

# ---------------------------------------------------------------- 4. run the installed app
asuser "cd / && setsid -f ~/.local/opt/fxsound/bin/FxSound >$RT/gui.log 2>&1 </dev/null"
sleep 7
pgrep -u $TU -x FxSound >/dev/null && pass "FxSound starts" || fail "FxSound does not start: $(tail -5 $RT/gui.log)"
LOG=/home/$TU/.config/FxSound/fxsound.log
grep -q "engine started" $LOG && pass "audio engine started ($(grep -o 'PipeWire [0-9.]*' $LOG | head -1))" || fail "engine did not start: $(tail -5 $LOG)"
i=0; while [ "$(defsink)" != '"name": "fxsound"' ] && [ $i -lt 20 ]; do sleep 0.5; i=$((i+1)); done
[ "$(defsink)" = '"name": "fxsound"' ] && pass "FxSound is the default output" || fail "default output is $(defsink)"
asuser "$IMPORT -window root '$SHOTS/distro_$LABEL.png'" 2>/dev/null
# a second start must not create a second FxSound (it brings the first window up instead)
asuser "cd / && setsid -f ~/.local/bin/fxsound >/dev/null 2>&1 </dev/null"
sleep 4
[ "$(pgrep -u $TU -x FxSound | wc -l)" = 1 ] && pass "single instance (second start reuses the running app)" || fail "second start: $(pgrep -u $TU -x FxSound | wc -l) FxSound processes"

# play the test signal into the default (FxSound) and record what reaches the speaker
asuser "cd /tmp/fxsound-install && (timeout 22 pw-record --target FakeSpeakers -P '{ stream.capture.sink=true }' --format f32 --rate 48000 --channels 2 $RT/rec.wav &) ; sleep 0.7; timeout 15 pw-play test_input.wav; sleep 1; pkill -INT -u $TU pw-record; sleep 1"
cp $RT/rec.wav "$OUT/distro_$LABEL.wav" 2>/dev/null && pass "recorded processed audio -> test/out/distro_$LABEL.wav" || fail "no recording"

# ---------------------------------------------------------------- 5. quit + uninstall
pkill -TERM -u $TU -x FxSound
sleep 2
pgrep -u $TU -x FxSound >/dev/null && fail "FxSound still running after SIGTERM" || pass "quits on SIGTERM"
[ "$(defsink)" = '"name": "FakeSpeakers"' ] && pass "default output restored to the speaker" || fail "default output after quit: $(defsink)"
grep -q 'name="preset" val="General"' /home/$TU/.config/FxSound/FxSound.settings 2>/dev/null && pass "settings saved" || fail "settings not saved"

asuser "~/.local/opt/fxsound/install.sh --uninstall -y" >/dev/null 2>&1
asuser "test ! -e ~/.local/opt/fxsound && test ! -e ~/.local/share/applications/fxsound.desktop && test ! -e ~/.local/bin/fxsound && test -d ~/.config/FxSound" \
    && pass "uninstall removes the app, keeps settings" || fail "uninstall left files"

# ---------------------------------------------------------------- 6. native package
if [ -n "$NATIVE" ]; then
    case "$NATIVE" in
        deb)  apt-get install -y -qq "$DIST/fxsound_${VER}_amd64.deb" >/tmp/fxsound-install/native.log 2>&1 ;;
        rpm)  if command -v dnf >/dev/null; then dnf install -y "$DIST/fxsound-$VER-1.x86_64.rpm" >/tmp/fxsound-install/native.log 2>&1
              else zypper -n --no-gpg-checks install --allow-unsigned-rpm "$DIST/fxsound-$VER-1.x86_64.rpm" >/tmp/fxsound-install/native.log 2>&1; fi ;;
        arch) pacman -U --noconfirm "$DIST"/fxsound-bin-$VER-1-x86_64.pkg.tar.zst >/tmp/fxsound-install/native.log 2>&1 ;;
    esac
    if [ -x /opt/fxsound/bin/FxSound ] && [ -f /usr/share/applications/fxsound.desktop ]; then
        pass "native $NATIVE package installs"
        asuser "cd / && setsid -f fxsound >$RT/gui2.log 2>&1 </dev/null"
        sleep 6
        pgrep -u $TU -x FxSound >/dev/null && [ "$(defsink)" = '"name": "fxsound"' ] && pass "packaged FxSound runs and takes over the output" || fail "packaged FxSound: $(tail -3 $LOG)"
        case "$NATIVE" in
            deb)  apt-get remove -y -qq fxsound >/dev/null 2>&1 ;;
            rpm)  if command -v dnf >/dev/null; then dnf remove -y -q fxsound >/dev/null 2>&1; else zypper -n -q remove fxsound >/dev/null 2>&1; fi ;;
            arch) pacman -R --noconfirm fxsound-bin >/dev/null 2>&1 ;;
        esac
        sleep 2
        [ ! -e /opt/fxsound ] && ! pgrep -u $TU -x FxSound >/dev/null && [ "$(defsink)" = '"name": "FakeSpeakers"' ] \
            && pass "native package removes cleanly, output restored" || fail "native removal: $(ls /opt 2>&1) $(defsink)"
    else
        fail "native $NATIVE package install: $(tail -5 /tmp/fxsound-install/native.log)"
    fi
fi

# ---------------------------------------------------------------- cleanup
pkill -u $TU -x FxSound 2>/dev/null; pkill -u $TU -x wireplumber; pkill -u $TU -x pipewire; pkill -u $TU -x dbus-daemon; [ "$DNUM" != 0 ] && pkill -f "Xvfb :$DNUM"
echo "=== $LABEL: $PASS passed, $FAIL failed"
