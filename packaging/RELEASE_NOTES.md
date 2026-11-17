# FxSound for Linux 1.2.15.2

FxSound for Linux, with the same app, presets and sound as FxSound 1.2.15 for Windows.

## Fixed in 1.2.15.2

* If your chosen output was unplugged (for example headphones), turning FxSound off or quitting now
  sends the sound back to a device that is still there, instead of leaving it on FxSound's device.

## Install

**One command (any distro):**

```
curl -fsSL https://raw.githubusercontent.com/tasin-py/FXSounds-Linux/main/get.sh | sh
```

**Any distro:** download `FxSound-1.2.15.2-linux-x86_64.tar.gz`, extract it, and run `./install.sh`
in the extracted folder. No root needed.

| Your distro | Download |
|---|---|
| Ubuntu 24.04+, Debian 13+, Mint 22+, Pop!_OS, Kali | `fxsound_1.2.15.2_amd64.deb` (double-click, or `sudo apt install ./fxsound_1.2.15.2_amd64.deb`) |
| Fedora 40+, openSUSE Tumbleweed | `fxsound-1.2.15.2-1.x86_64.rpm` (`sudo dnf install ./fxsound-1.2.15.2-1.x86_64.rpm` / `sudo zypper install ./...`) |
| Arch, Manjaro, EndeavourOS | `fxsound-bin-1.2.15.2-1-x86_64.pkg.tar.zst` (`sudo pacman -U ./fxsound-bin-...`) |

Requires a 64-bit PC and PipeWire (the default sound system on all of the above).

## What's in it

* The full FxSound app: 10-band EQ, Clarity, Ambience, Surround Sound, Dynamic Boost, Bass Boost
  and all presets. Save your own presets, plus rename, delete, export and import.
* The same sound processing as Windows: output tested to match the Windows build exactly.
* FxSound becomes your default output while it is on, and your normal device comes back when you
  turn it off or quit. It follows headphones being plugged in and output changes in system settings.
* Tray icon, launch on login, and keyboard shortcuts on X11 (Ctrl+Shift+Q on/off, A/Z next/previous
  preset, W next output, E show/hide).
* New on Linux: **Check for updates**, **Copy Log (for bug reports)**, and a ♥ menu where you choose
  whom to support: the Linux port developer, or the FxSound core developers.

## Tested on

Arch Linux, Fedora 44, Kali Rolling, Debian 13, Ubuntu 24.04, Ubuntu 26.04, openSUSE Tumbleweed.
On each one: install, start, sound processing (checked sample by sample against the reference),
handing the default output over and back, single instance, settings, quit and uninstall.

## Known limitations

* Global keyboard shortcuts work on X11 only. Wayland does not let apps grab keys globally; use your
  desktop's own shortcut settings with `fxsound --power=0` / `fxsound --power=1` instead.
* On GNOME, the tray icon needs the "AppIndicator and KStatusNotifierItem Support" extension
  (preinstalled on Ubuntu).
