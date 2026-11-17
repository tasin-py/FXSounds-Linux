/*
 * FxLinuxTray.h - system tray icon for Linux.
 *
 * Uses the StatusNotifierItem / AppIndicator protocol (KDE, GNOME with the AppIndicator extension,
 * Ubuntu, Cinnamon, XFCE, MATE, LXQt, Budgie...) through libayatana-appindicator / libappindicator,
 * loaded at runtime so FxSound still starts where they are missing. Falls back to the classic
 * X11 system tray (XEmbed) through JUCE.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#pragma once
#include <JuceHeader.h>

class FxLinuxTray
{
public:
    // Builds the menu to show (called whenever the menu must be refreshed / opened).
    using MenuBuilder = std::function<void(juce::PopupMenu &)>;

    FxLinuxTray(MenuBuilder builder, std::function<void()> on_activate);
    ~FxLinuxTray();

    void setStatus(bool power, bool processing, const juce::String &tooltip);
    void refreshMenu();
    bool isAvailable() const;   // a tray host is present (icon visible)
    juce::String backendName() const;

    // Window/tray icon matching the Windows app: red (dark theme) / blue (light) when processing,
    // white when on but idle, gray when off.
    static juce::Image iconImage(bool power, bool processing);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
