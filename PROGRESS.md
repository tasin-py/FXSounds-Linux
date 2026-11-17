# FxSound Linux port — progress log

Resume point for future sessions. Update after every milestone.

## Layout
- `upstream/` — copied from fxsound2/fxsound-app @ d6d5b740 (dsp/, audiopassthru/, presets/ = bin/x64/Factsoft/*.fac).
  Linux edits are marked `FX_LINUX` (except the mechanical long->pt_long conversion).
- `compat/` — Win32 shim: `fx_linux_compat.h` (force-included), stub headers, `win32_misc.cpp`, `registry.cpp` (in-memory registry).
- `tools/gen_case_links.sh` — case-correct header symlinks (upstream includes have inconsistent case).
- `tools/convert_long.py` — rewrote `long` -> `pt_long` (int32) in DSP + helper sources (2135 sites). Already applied; idempotent.
- `tools/wsl-build.sh` — copies project to `~/fxsound-linux` in WSL (native case-sensitive fs) and runs make.
  `wsl -d archlinux -- sh "/mnt/f/Claude Devs/FXSounds Linux/tools/wsl-build.sh" -j12`   (add `SAN=1` for ASan/UBSan build in build-san/)
- `tools/fxprocess.cpp` — CLI: WAV in -> DSP (preset/sliders/EQ) -> float WAV out; `--info` prints state.
- `test/` — make_test_wav.py, presets_info.sh, process_all.sh, spectrum_compare.py, bench.sh.
- Build env: WSL archlinux has gcc/g++/make/gdb (no cmake, rsync, bc, pipewire-dev yet).
- PowerShell -> wsl quoting is fragile: put shell commands in .sh files and run them.

## Bugs found & fixed (important for the full port)
1. `long` is 64-bit on Linux; DSP assumes 32-bit (writes floats through long*) -> convert_long.py.
2. Pointer comparisons via `(long)` casts in Dly8/Lex delay+reverb truncated pointers -> `(intptr_t)`.
3. libstdc++ <cwchar>/<cstdio> `#undef` C names -> compat header includes them before defining macros.
4. glibc forbids mixing fgets/fgetws on one FILE; CRLF presets -> fx_fgets/fx_fgetws (UTF-8, CRLF->LF).
5. MSVC wide printf/scanf semantics (%s = wide) -> fx_ms_* format translation; swprintf without size overload.
6. Upstream: DfxDspPrivate members uninitialized (preset_list_handle_ freed in dtor -> crash) -> init in ctor.
7. Upstream: isPowerOn() inverted -> fixed (Windows GUI never calls it).
8. Makefile needed -MMD header deps (stale struct layouts caused a false overflow).

9. Upstream (Windows too): pristine reference crashes on exit intermittently (same uninitialized member).

## Null test vs Windows (done 2026-11-17)
- `reference/upstream-pristine/` = unmodified upstream dsp+audiopassthru; `reference/windows-fxsound-1.2.16/` = installed app copy
  (factory presets identical to repo + Default.fac, now in upstream/presets); `reference/windows-user-data/` = user's presets/settings.
- `tools/build-windows-reference.ps1` builds build-win/fxprocess.exe from pristine source with MSVC 14.50 (VS 18 BuildTools).
  fxprocess on Windows sandboxes HKCU (RegOverridePredefKey) — the DSP WRITES HKCU\Software\DFX session state otherwise.
  NOTE: early reference runs (before the sandbox) did overwrite the user's HKCU\Software\DFX\13\23 LASTUSED values; told user.
- `python test/null_test.py` (after process_all.sh + tools/wsl-fetch-out.sh): bypass, sliders-only and all 12 presets are
  BIT-IDENTICAL Linux vs Windows.
- compat/registry.cpp supports FX_REGISTRY_IMPORT=<tsv> (test/fixtures/windows_session.tsv) to preload registry values.

## Phase 2: PipeWire engine (done 2026-11-17)
- `engine/fx_engine.{h,cpp}` = FxEngine: virtual sink "fxsound" (capture stream, media.class Audio/Sink) + playback
  stream "fxsound_output" -> real device. Same scheme as pipewire module-loopback 1.6: shared node.group/link-group,
  playback connected FIRST with PW_STREAM_FLAG_TRIGGER, capture with PW_STREAM_FLAG_ASYNC, capture_process triggers
  playback, playback_process dequeues both and runs the DSP (mutex shared with GUI setters). Without ASYNC/TRIGGER
  order ~40% of audio was lost (bug found+fixed).
- Default device: claims default.configured.audio.sink, restores on stop (or the active device if a previous run
  left "fxsound" as default). Follows user picking another device in system settings (moves output there, re-claims;
  gives up after 5 reclaims/10s to avoid fighting other tools). Device unplug -> next best sink. setOutputDevice().
- connectionLost() after PipeWire restart -> stop()+start() (CLI does it automatically, tested).
- `engine/fxsound_engine_main.cpp` = build/fxsound-engine CLI (stdin commands: preset/power/effect/eq/output/devices/status/quit).
- Tests (need headless PipeWire in WSL: scratchpad pwup.sh = setsid pipewire/wireplumber/pipewire-pulse +
  null sink FakeSpeakers; XDG_RUNTIME_DIR=/tmp/pwtest; setsid is required or wireplumber dies on SIGHUP):
  live recording via pw-record of FakeSpeakers monitor is BIT-IDENTICAL to offline fxprocess render
  (test/live_compare.py), 0 dropouts; routing scenarios OK; test/stress_engine.sh 3000 random changes under ASan clean.

## Phase 3: GUI (done 2026-11-17)
- Upstream JUCE GUI built on Linux with CMake (JUCE 7.0.12 in ~/third_party/JUCE). `tools/port_gui.py` copies
  reference/upstream-pristine/fxsound/Source -> upstream/fxsound/Source and applies tools/port_gui_edits*.py
  (all Linux changes inside `#if JUCE_LINUX`). NEVER edit upstream/fxsound/Source by hand - edit the edit lists.
- linux/: AudioPassthru on FxEngine, FxLockedDsp (thread-safe DSP), log (~/.config/FxSound/fxsound.log), donate menu
  (port dev + core devs), update check (UPDATE_REPO in linux/FxLinuxFeatures.cpp - EMPTY until the user gives
  the repo; env FXSOUND_UPDATE_REPO overrides), Copy Log, XDG autostart, X11 hotkeys (off on Wayland), tray
  (AppIndicator via dlopen, JUCE XEmbed fallback), SIGTERM/SIGINT/SIGHUP -> normal quit (restores default output).
- GUI live audio is bit-identical to `fxprocess --gui-init` (and to the Windows reference build).
- Bugs fixed: EQ curve not redrawn while dragging / after preset change by hotkey (upstream bugs); preset list order
  (NTFS sorted vs Linux unsorted); "Launch on startup" hidden (upstream shows it only on Win7); autostart Exec path
  wrong (argv[0] relative after chdir -> /proc/self/exe); settings path ~/.config/FxSound.

## Phase 4: packaging
- `tools/build-release.sh` (run on Ubuntu 24.04 WSL = oldest glibc 2.39 / PipeWire 1.0.5) -> dist/:
  FxSound-<ver>-linux-x86_64.tar.gz (+ packaging/install.sh: per-user ~/.local/opt/fxsound, --system, --uninstall),
  fxsound_<ver>_amd64.deb, fxsound-<ver>-pkgroot.tar.gz (input for packaging/rpm/fxsound.spec and packaging/arch/PKGBUILD).
- AppImage NOT built: needs appimagetool downloaded from GitHub -> needs the user's OK.
- `test/distro_test.sh <label> <display> [deb|rpm|arch]` = full install/run/audio/quit/uninstall test per distro.

## Phase 5: distro tests (2026-11-17)
- test/distro_test.sh as root per WSL distro (creates user fxtest, real user install from the tarball, headless
  PipeWire + null sink + Xvfb, 13 checks, native package install/run/remove). Run a COPY of the script (editing it
  while it runs breaks the run). Audio: python test/compare_distro.py (Windows python has numpy) vs
  test/out/ref_general_gui.wav -> max diff 2.3e-26 (float rounding), 0 dropouts.
- FINAL (packages built 12:52): 13/13 on Arch, Fedora 44, Kali, Ubuntu 24.04 (PW 1.0.5), Ubuntu 26.04, openSUSE TW
  (on WSLg :0 - Xvfb conflicts with patterns-wsl-tmpfiles, not installed on purpose), archtest; Debian 13: 11/13 only
  because WSL-Debian's D-Bus makes ANY PipeWire client (pw-cli too) take ~25 s to connect (environment; earlier run
  without session bus 13/13). Audio identical (2.3e-26) on 7 distros; Ubuntu 24.04 -85 dB residual growing to -23 dB
  in the last second = PipeWire 1.0 rate-matching resampler in the headless graph (same binary elsewhere identical).
- Bugs found by these tests: JUCE single-instance lock in /var/tmp shared by all users (fixed: per-user lock+socket
  in $XDG_RUNTIME_DIR, second start shows the window / forwards --power=0 etc.); symlink launcher made the process
  "fxsound" so pkill -x FxSound missed it (fixed: exec wrapper); CRLF in CMakeLists broke version parsing;
  rpm spec unquoted path with space; GitHub API rate limit (fixed: releases/latest redirect first).
- CLI options need "=": fxsound --power=0 (JUCE ArgumentList long options).
- Upstream crash fixed: quitting (SIGTERM/logout) with the preset dropdown open -> ~ComboBox dismisses popup ->
  mouseEnter on half-destroyed FxView -> SIGSEGV. Fix: dismissAllActiveMenus + cancelAllModalComponents at the
  start of shutdown(). Crash found by test: no display (DISPLAY set, no X server) -> AlertWindow null monitor ->
  SIGSEGV; fix: FxLinux::hasDisplay() check, clean exit 1 with message.
- Test harness: WirePlumber 0.4 (Ubuntu 24.04) needs a session D-Bus (distro_test starts a private one); null sink
  forced to F32 (PipeWire 1.0 null sink otherwise converts -> -85 dB residual, not a DSP difference).

## Status
- [x] Phase 1 PoC (2026-11-17): DSP builds on Linux, ASan/UBSan clean, all 12 presets load with names,
      bypass is bit-exact, presets shape spectrum as expected, ~100x realtime.
- [x] Null test vs Windows: bit-identical (see above).
- [x] Phase 2 PipeWire engine.
- [x] Phase 3 JUCE GUI (same UI; 2-way donate; update check — repo URL TBD; copy-log button; save custom EQ).
- [x] Phase 4 packaging (tar.gz+install.sh, deb, rpm, Arch). AppImage script ready, needs appimagetool (user OK).
- [x] Phase 5 test on WSL distros (all 8).
- [ ] USER: GitHub repo -> UPDATE_REPO in linux/FxLinuxFeatures.cpp (+ Homepage in packaging/deb/control, rpm spec,
      PKGBUILD url), confirm donate label name "Tasin", deb Maintainer email, publish (AGPL: source must be public),
      check on a real desktop (tray on GNOME/KDE, Wayland, real speakers/headphones).

## Phase 6: real desktop VM + repo (2026-11-17)
- Repo: https://github.com/tasin-py/FXSounds-Linux -> UPDATE_REPO, deb Homepage, rpm URL, PKGBUILD url+source
  (release asset URL, tag v<ver>). get.sh = one-line installer (curl ... | sh; --user, --uninstall; picks
  deb/rpm/arch, verifies SHA256SUMS from the release). tools/build-native-pkgs.sh rpm|arch|sums.
  Release = tag "v1.2.15.1" with all files from dist/ incl. SHA256SUMS.
- VMware VM vm/ArchFx (Arch + KDE Plasma 6 Wayland, HD Audio card, bridged net; VMware services must be started as
  admin: vm/start-vmware-services.cmd; NAT/DHCP services were still stopped -> bridged). SSH: vm/id_fxvm,
  tester password in vm/CREDENTIALS.txt; vm/sessenv.sh copies the FULL Plasma session env over SSH (a partial env lacked KDE_SESSION_VERSION -> xdg-open silently failed); vm/traymenu.py lists/clicks tray menu items via dbusmenu (no xdotool prompt); screenshots with
  vmrun captureScreen (needs -gu tester -gp <pw>). xdotool clicks trigger a KDE "Remote Control" permission prompt.
  NEVER run "python -" in Git Bash here: it opens an interactive Windows python and hangs.
- Results: real device name shown; audio bit-identical on the HDA sink (vm/audio_test.sh); switching output in the
  volume menu, unplug fallback (vm/switch_test.sh) OK.
- BUG fixed: no tray icon on KDE Wayland without libayatana-appindicator (XEmbed fallback is invisible there) ->
  now a hard dependency in deb/rpm/arch; install.sh warns with the install command.
- BUG fixed: donate links / update page did nothing when no browser is registered (xdg-open exits 0) ->
  FxLinux::openUrl checks xdg-mime, else copies the link and shows a message.
- Donate links verified in Firefox (both options open the right page via tray menu).
- PUBLISHED 2026-11-17: code pushed to github.com/tasin-py/FXSounds-Linux (main), release v1.2.15.1 with
  6 assets (tar.gz, deb, rpm, arch pkg, pkgroot, SHA256SUMS). gh CLI installed + logged in as tasin-py.
  Local git repo in the project folder (author "Tasin's Problems" <101010074+tasin-py@users.noreply.github.com>);
  .gitignore keeps vm/, dist/, Windows app copy/user data, test recordings out of git.
- get.sh verified from the live release: Arch VM (pacman), Ubuntu 26.04 (deb + --uninstall), Fedora --user.
  releases/latest redirect -> tag v1.2.15.1 (update check reads this; same as installed = "up to date").
- New release: bump FX_VERSION in CMakeLists.txt, sh tools/build-release.sh (Ubuntu-24.04 WSL),
  tools/build-native-pkgs.sh rpm (FedoraLinux-44, root) and arch (archtest, root) -> dist/ + SHA256SUMS,
  then gh release create v<ver> dist/* --notes-file packaging/RELEASE_NOTES.md.
