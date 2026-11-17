# FxSound for Linux

FxSound makes everything you play sound better: 10-band equalizer, Clarity, Ambience,
Surround Sound, Dynamic Boost and Bass Boost, with the same presets and the same sound
processing as the Windows app.

## Install with one command

```
curl -fsSL https://raw.githubusercontent.com/tasin-py/FXSounds-Linux/main/get.sh | sh
```

It picks the right package for your distro. Downloads: https://github.com/tasin-py/FXSounds-Linux/releases

## Install from the archive (any distro)

1. Extract the archive (right-click → *Extract here*, or `tar xzf FxSound-*-linux-x86_64.tar.gz`).
2. Open a terminal in the extracted folder and run:

   ```
   ./install.sh
   ```

That's it. FxSound is now in your applications menu. No root password is needed: everything
goes into your home folder (`~/.local/opt/fxsound`), and nothing on your system is changed.

Using Debian, Ubuntu, Mint, Pop!_OS or Kali? You can instead double-click the `.deb` file.
On Fedora or openSUSE, use the `.rpm` file. On Arch, Manjaro or EndeavourOS, use the `.pkg.tar.zst` file
(`sudo pacman -U ./fxsound-bin-*.pkg.tar.zst`).

## Requirements

* A 64-bit PC (x86_64).
* A current distro that uses **PipeWire** for sound. That covers Ubuntu 24.04+, Debian 13+,
  Fedora 40+, Arch, Manjaro, EndeavourOS, openSUSE Tumbleweed, Kali, Linux Mint 22+ and Pop!_OS 24.04+.

## How it works

While FxSound is on, it adds an output device called **FxSound** and makes it your default.
Your apps play into FxSound, which processes the sound and sends it to your real speakers
or headphones (choose which ones in the device list at the top right of the app).
When you turn FxSound off or quit it, your normal output device becomes the default again.

## Uninstall

```
~/.local/opt/fxsound/install.sh --uninstall
```

Your settings and saved presets in `~/.config/FxSound` are kept unless you say otherwise.

## Problems?

Open the menu (☰) and choose **Copy Log (for bug reports)**, then paste the copied text into
your bug report. The log is also saved in `~/.config/FxSound/fxsound.log`.

## Support the people behind it

The ♥ button in the app lets you choose whom to support: the developer of this Linux port,
or the FxSound core developers, who made the original app.

FxSound is free and open-source software (GNU AGPL v3).
