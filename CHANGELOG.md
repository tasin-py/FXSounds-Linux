# Changelog

All notable changes to FxSound for Linux. Versions are the upstream FxSound version (1.2.15) plus a
Linux port revision. Downloads are on the [Releases](https://github.com/tasin-py/FXSounds-Linux/releases) page.

## Unreleased

### Docs

* README: new **Uninstall** section. Remove FxSound on any distro with one command:

  ```sh
  curl -fsSL https://raw.githubusercontent.com/tasin-py/FXSounds-Linux/main/get.sh | sh -s -- --uninstall
  ```

  or with your package manager (`sudo apt remove fxsound`, `sudo dnf remove fxsound`,
  `sudo zypper remove fxsound`, `sudo pacman -R fxsound-bin`). Settings and presets are kept; delete
  `~/.config/FxSound` to remove them too.

## 1.2.15.2 (2026-11-17)

### Fixed

* If your chosen output was unplugged (for example headphones), turning FxSound off or quitting now
  sends the sound back to a device that is still there, instead of leaving it on FxSound's device.

### Docs

* New README with an animated banner, badges, a screenshot, and usage and troubleshooting sections.

## 1.2.15.1 (2026-11-17)

First release of FxSound for Linux.

* The full FxSound 1.2.15 app: 10-band EQ, Clarity, Ambience, Surround Sound, Dynamic Boost, Bass Boost
  and all presets. Save, rename, delete, export and import your own presets.
* Sound processing bit-identical to the Windows build.
* PipeWire virtual output **FxSound**: becomes the default while on, hands your device back when off or
  quit, and follows device changes and unplugged headphones.
* Tray icon (StatusNotifier, works on KDE Wayland), launch on login, keyboard shortcuts on X11, and a
  command line (`--power=0|1`, `--preset="Name"`).
* New on Linux: **Check for updates**, **Copy Log**, and a ♥ menu to support the Linux port developer or
  the FxSound core developers.
* Packages: `.deb`, `.rpm`, Arch `.pkg.tar.zst`, a tarball with `install.sh`, and the one-line installer
  `get.sh` (`--user`, `--uninstall`).
* Tested on Arch, Fedora 44, Kali, Debian 13, Ubuntu 24.04 and 26.04, openSUSE Tumbleweed, and KDE
  Plasma (Wayland) on real hardware audio.
