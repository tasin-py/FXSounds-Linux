# FxSound for Linux

A Linux port of [FxSound](https://github.com/fxsound2/fxsound-app), the free and open-source audio enhancer.
It has the same interface and presets as the Windows app, and the same sound: the DSP output is
**bit-identical** to the Windows build.

* 10-band equalizer, Clarity, Ambience, Surround Sound, Dynamic Boost and Bass Boost.
* All presets from the Windows app. Save, rename, export and import your own.
* PipeWire native. FxSound adds an *FxSound* output device, makes it the default while it is on,
  and gives your normal device back when you turn it off or quit.
* Follows your device changes: plugging in headphones, picking another output in system settings,
  or restarting PipeWire.
* Tray icon (AppIndicator / StatusNotifier, with an XEmbed fallback), global hotkeys on X11,
  launch on login.
* Linux extras: **Check for updates** (GitHub releases), **Copy Log** for bug reports, and a donate
  menu that lets you choose between the Linux port developer and the FxSound core developers.

## Install

Open a terminal and paste:

```sh
curl -fsSL https://raw.githubusercontent.com/tasin-py/FXSounds-Linux/main/get.sh | sh
```

It downloads the latest release and installs the right package for your distro (it asks for your
password). Add `-s -- --user` after `sh` to install just for you without a password, or `-s -- --uninstall`
to remove FxSound. You can also download a package yourself from the
[Releases page](https://github.com/tasin-py/FXSounds-Linux/releases):

For more install details see [packaging/README.md](packaging/README.md).

| File | For |
|---|---|
| `FxSound-<ver>-linux-x86_64.tar.gz` | any distro: extract, run `./install.sh` (no root) |
| `fxsound_<ver>_amd64.deb` | Debian 13+, Ubuntu 24.04+, Mint 22+, Pop!_OS, Kali |
| `fxsound-<ver>-1.x86_64.rpm` | Fedora 40+, openSUSE Tumbleweed |
| `fxsound-bin-<ver>-1-x86_64.pkg.tar.zst` | Arch, Manjaro, EndeavourOS (`sudo pacman -U ...`) |

Requires x86_64, glibc 2.39+ and PipeWire.

## Building

```sh
# deps (Ubuntu/Debian names)
sudo apt install build-essential cmake ninja-build pkg-config python3 libpipewire-0.3-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libfreetype-dev \
  libfontconfig1-dev libcurl4-openssl-dev
git clone --depth 1 --branch 7.0.12 https://github.com/juce-framework/JUCE ~/third_party/JUCE

sh tools/build.sh                 # development build: build-gui/FxSound_artefacts/Release/FxSound
sh tools/build-release.sh dist    # release tarball + .deb (build on the oldest distro you support)
```

The `.rpm` comes from `packaging/rpm/fxsound.spec` (via `rpmbuild`), and the Arch package from
`packaging/arch/PKGBUILD` (via `makepkg`). Both use `dist/fxsound-<ver>-pkgroot.tar.gz`.

## Layout

| Path | What |
|---|---|
| `upstream/` | FxSound sources (DSP, GUI, presets, fonts). The GUI in `upstream/fxsound/Source` is generated: `tools/port_gui.py` copies the pristine upstream sources and applies the edits in `tools/port_gui_edits*.py`. All Linux changes there are inside `#if JUCE_LINUX`. |
| `compat/` | Windows API shims for the DSP (registry, file and string functions). |
| `engine/` | PipeWire engine: the virtual *FxSound* sink, playback to the real device, default-device handling. `build/fxsound-engine` is a CLI for testing. |
| `linux/` | Linux side of the GUI: AudioPassthru on the engine, tray, hotkeys, log, update check, donate menu, autostart. |
| `packaging/` | installer, desktop entry, deb/rpm/Arch packaging. |
| `test/` | null tests against Windows output, live PipeWire recording tests, `distro_test.sh` (full install/run/uninstall test per distro). |

## License

GNU Affero General Public License v3.0 or later, like upstream FxSound. See [LICENSE](LICENSE).
FxSound is © FxSound LLC and its contributors. The Linux port is by Tasin.
