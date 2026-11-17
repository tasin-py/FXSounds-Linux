/*
 * FxLinuxFeatures.h - Linux-only features of FxSound for Linux:
 *   - two-way donate menu (the Linux port maintainer and the FxSound core developers)
 *   - update check against the GitHub releases of the Linux port
 *   - copy log / diagnostic report to the clipboard
 *   - launch on login (XDG autostart)
 *   - global hotkeys (X11)
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#pragma once
#include <JuceHeader.h>

namespace FxLinux
{
    // ---- configuration (edit here) ----
    // GitHub repository of the Linux port, "owner/name". Empty = update check not configured yet.
    extern const char *UPDATE_REPO;
    extern const char *PORT_DONATE_URL;      // Linux port maintainer
    extern const char *CORE_DONATE_URL;      // FxSound core developers (upstream)

    // Opens a web link in the default browser. When no browser is set up (xdg-open then fails
    // silently), copies the link to the clipboard and tells the user instead of doing nothing.
    void openUrl(const juce::String &url);

    // Shows the two donate choices as a popup menu (under `target` if given).
    void showDonateMenu(juce::Component *target = nullptr);
    void addDonateItems(juce::PopupMenu &menu);

    // interactive = user clicked "Check for updates" (always shows a result);
    // otherwise silent unless a newer version exists (daily automatic check).
    void checkForUpdates(bool interactive);

    // Copies a diagnostic report + the log to the clipboard and tells the user.
    void copyLogToClipboard();

    bool isAutostartEnabled();
    void setAutostartEnabled(bool enable);

    // Data directory with Factsoft/ presets and fonts (next to the binary, or ../share/fxsound).
    juce::File getAppDataDir();

    bool isWayland();

    // False (after logging why and printing it to stderr) when no X11/XWayland display can be opened.
    bool hasDisplay();

    // Windows (NTFS) lists a folder's files sorted by name, case-insensitively; Linux file systems
    // return them in no fixed order. Sorting keeps the preset list in the same order as on Windows.
    void sortFilesLikeWindows(juce::Array<juce::File> &files);

    // Per-user single instance (lock + socket in $XDG_RUNTIME_DIR). Returns true for the first
    // instance, which then receives the command lines of later starts (-> applyConfig, which shows
    // the window). A later start forwards its command line and gets false: it must exit at once.
    bool claimSingleInstance(const juce::String &commandline);

    // SIGTERM/SIGINT/SIGHUP (logout, shutdown, Ctrl+C) -> normal quit: saves the preset and gives
    // the system its default output device back.
    void installQuitSignalHandlers();

    namespace Hotkeys
    {
        // mod: MOD_ALT|MOD_CONTROL|MOD_SHIFT (Windows values), vk: '0'-'9' / 'A'-'Z'
        bool registerHotkey(int id, int mod, int vk);
        void unregisterHotkey(int id);
        bool isSupported();
        // Calls back on the message thread with the hotkey id.
        void setHandler(std::function<void(int)> handler);
    }
}
