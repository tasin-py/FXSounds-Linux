# FxSound for Linux - RPM (Fedora, openSUSE, RHEL-likes with glibc >= 2.39).
# Built from the prebuilt package tree made by tools/build-release.sh:
#   rpmbuild -bb --define "_sourcedir $PWD/dist" --define "fxversion 1.2.15.2" packaging/rpm/fxsound.spec
%{!?fxversion: %global fxversion 1.2.15.2}
%global debug_package %{nil}
%global __strip /bin/true
# the bundled fonts/presets are data, not ELF
%global __requires_exclude_from ^/opt/fxsound/share/.*$

Name:           fxsound
Version:        %{fxversion}
Release:        1
Summary:        FxSound audio enhancer for Linux (PipeWire)
License:        AGPL-3.0-or-later
URL:            https://github.com/tasin-py/FXSounds-Linux
Source0:        fxsound-%{fxversion}-pkgroot.tar.gz
ExclusiveArch:  x86_64
Recommends:     wireplumber
Recommends:     libcurl.so.4()(64bit)
# soname requires work on Fedora and openSUSE alike (JUCE loads these at runtime)
Requires:       libXext.so.6()(64bit) libXrandr.so.2()(64bit) libXinerama.so.1()(64bit) libXcursor.so.1()(64bit)
# tray icon (StatusNotifierItem); KDE and GNOME show no XEmbed icons on Wayland
Requires:       libayatana-appindicator3.so.1()(64bit)

%description
FxSound improves the sound of everything you play: 10-band equalizer, clarity,
ambience, surround, dynamic boost and bass boost, with the presets of the
Windows app. It adds an "FxSound" output device to PipeWire and plays the
enhanced sound on your speakers or headphones.

This is the Linux port of the open-source FxSound app.

%prep
%setup -q -c -T
tar -xzf "%{SOURCE0}"

%build

%install
cp -a . %{buildroot}/

%preun
# on removal (not upgrade) quit running FxSound windows so the normal output device comes back
if [ "$1" = 0 ]; then pkill -TERM -x FxSound 2>/dev/null || true; fi

%files
/opt/fxsound
/usr/bin/fxsound
/usr/share/applications/fxsound.desktop
/usr/share/icons/hicolor/*/apps/fxsound.png
%license /usr/share/licenses/fxsound/LICENSE
