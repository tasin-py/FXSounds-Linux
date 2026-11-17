# Second part of the edit list for tools/port_gui.py (main window, tray, SysInfo, Main.cpp)

# ---------------------------------------------------------------- FxMainWindow.cpp
edit('GUI/FxMainWindow.cpp', [
    ('#include "FxPresetNameEditor.h"\n', '#include "FxPresetNameEditor.h"\n#if JUCE_LINUX\n#include "FxLinuxFeatures.h" // FX_LINUX\n#include "FxLinuxTray.h"\n#endif\n'),
    ('''	donate_button_.onClick = [this]() {
		URL url("https://www.paypal.com/donate/?hosted_button_id=JVNQGYXCQ2GPG");
		url.launchInDefaultBrowser();
	};''', '''	donate_button_.onClick = [this]() {
#if JUCE_LINUX
		FxLinux::showDonateMenu(&donate_button_); // FX_LINUX: Linux port maintainer or FxSound core developers
#else
		URL url("https://www.paypal.com/donate/?hosted_button_id=JVNQGYXCQ2GPG");
		url.launchInDefaultBrowser();
#endif
	};'''),
    ('''	auto* peer = getPeer();
	if (peer)
	{
		HWND hwnd = (HWND)peer->getNativeHandle();
		if (IsIconic(hwnd)) // If minimized, restore first''', '''	auto* peer = getPeer();
#if JUCE_LINUX
	if (peer)
	{
		if (peer->isMinimised())
			peer->setMinimised(false);
		peer->toFront(true);
	}
#else
	if (peer)
	{
		HWND hwnd = (HWND)peer->getNativeHandle();
		if (IsIconic(hwnd)) // If minimized, restore first'''),
    ('''		SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	}
}''', '''		SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	}
#endif
}'''),
    ('''void FxMainWindow::setIcon(bool power, bool processing)
{
	HINSTANCE hInst = GetModuleHandle(NULL);''', '''void FxMainWindow::setIcon(bool power, bool processing)
{
#if JUCE_LINUX
	if (auto* peer = getPeer())
		peer->setIcon(FxLinuxTray::iconImage(power, processing));
#else
	HINSTANCE hInst = GetModuleHandle(NULL);'''),
    ('''	if (curr_icon != NULL)
	{
		DestroyIcon(curr_icon);
	}
}''', '''	if (curr_icon != NULL)
	{
		DestroyIcon(curr_icon);
	}
#endif
}'''),
    ('''void FxMainWindow::visibilityChanged()
{
	auto* peer = getPeer();''', '''void FxMainWindow::visibilityChanged()
{
#if JUCE_WINDOWS
	auto* peer = getPeer();'''),
    ('''			SetWindowLong(hwnd, GWL_STYLE, style | WS_MINIMIZEBOX);
		}
	}
}''', '''			SetWindowLong(hwnd, GWL_STYLE, style | WS_MINIMIZEBOX);
		}
	}
#endif
}'''),
    ('''	auto checkForUpdatesClicked = []() {
		ChildProcess child_process;
		child_process.start("updater.exe /checknow");
	};''', '''	auto checkForUpdatesClicked = []() {
#if JUCE_LINUX
		FxLinux::checkForUpdates(true);
#else
		ChildProcess child_process;
		child_process.start("updater.exe /checknow");
#endif
	};
#if JUCE_LINUX
	auto copyLogClicked = []() {
		FxLinux::copyLogToClipboard();
	};
#endif'''),
    ('''	popup_menu.addItem(TRANS("Check for updates"), checkForUpdatesClicked);
	popup_menu.addSeparator();''', '''	popup_menu.addItem(TRANS("Check for updates"), checkForUpdatesClicked);
#if JUCE_LINUX
	popup_menu.addItem(TRANS("Copy Log (for bug reports)"), copyLogClicked);
#endif
	popup_menu.addSeparator();'''),
    ('''	popup_menu.addItem(TRANS("Donate"), donateClicked);
''', '''#if JUCE_LINUX
	PopupMenu donate_menu;
	FxLinux::addDonateItems(donate_menu);
	popup_menu.addSubMenu(TRANS("Donate"), donate_menu);
	(void)donateClicked;
#else
	popup_menu.addItem(TRANS("Donate"), donateClicked);
#endif
'''),
    ('''			ShowWindow((HWND)getWindowHandle(), SW_MINIMIZE);''', '''#if JUCE_LINUX
			if (auto* peer = getPeer()) peer->setMinimised(true);
#else
			ShowWindow((HWND)getWindowHandle(), SW_MINIMIZE);
#endif'''),
])

# ---------------------------------------------------------------- FxSystemTrayView (Linux implementation: linux/FxSystemTrayViewLinux.cpp)
edit('GUI/FxSystemTrayView.h', [
    ('''	static constexpr int WMAPP_FXTRAYICON = WM_APP + 1;
	static const GUID trayIconGuid_;

	static LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
''', '''#if JUCE_WINDOWS
	static constexpr int WMAPP_FXTRAYICON = WM_APP + 1;
	static const GUID trayIconGuid_;

	static LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#else
public:
	bool isTrayAvailable() const; // FX_LINUX
	void buildContextMenu(PopupMenu& context_menu);
private:
	std::unique_ptr<class FxLinuxTray> tray_;
#endif
'''),
    ('''	bool custom_notification_;
	FxNotification notification_;
	WNDPROC componentWndProc_;
	UINT taskbar_created_message_;''', '''	bool custom_notification_;
	FxNotification notification_;
#if JUCE_WINDOWS
	WNDPROC componentWndProc_;
	UINT taskbar_created_message_;
#endif'''),
])
edit('GUI/FxSystemTrayView.cpp', [
    ('#include <JuceHeader.h>\n#include "FxSystemTrayView.h"', '#include <JuceHeader.h>\n#if JUCE_WINDOWS // FX_LINUX: see linux/FxSystemTrayViewLinux.cpp\n#include "FxSystemTrayView.h"'),
])
p = os.path.join(ROOT, 'GUI/FxSystemTrayView.cpp')
s = open(p, encoding='utf-8').read()
if not s.rstrip().endswith('#endif // JUCE_WINDOWS'):
    open(p, 'w', encoding='utf-8', newline='\n').write(s.rstrip() + '\n#endif // JUCE_WINDOWS\n')

# ---------------------------------------------------------------- SysInfo.cpp is Windows only
p = os.path.join(ROOT, 'Utils/SysInfo/SysInfo.cpp')
s = open(p, encoding='utf-8-sig').read()
if not s.startswith('#ifdef _WIN32'):
    open(p, 'w', encoding='utf-8', newline='\n').write('#ifdef _WIN32 // FX_LINUX: see linux/SysInfoLinux.cpp\n' + s.rstrip() + '\n#endif\n')

# ---------------------------------------------------------------- Main.cpp
edit('Main.cpp', [
    ('''#include <comdef.h>
#include "GUI/FxSystemTrayView.h"''', '''#if JUCE_WINDOWS
#include <comdef.h>
#endif
#include "GUI/FxSystemTrayView.h"'''),
    ('''#include "AudioPassthru.h"
#include <dbghelp.h>''', '''#include "AudioPassthru.h"
#if JUCE_LINUX
#include "FxLinuxFeatures.h" // FX_LINUX
#else
#include <dbghelp.h>'''),
    ('''    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0;
}''', '''    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0;
}
#endif'''),
    ('''            SetUnhandledExceptionFilter(unhandledExceptionFilter);

            HRESULT hRes = CoInitializeEx(0, COINIT_MULTITHREADED);''', '''#if JUCE_LINUX
            SystemStats::setApplicationCrashHandler(linuxCrashHandler);
            Logger::setCurrentLogger(FxLinuxLog::logger());
            FxLinux::installQuitSignalHandlers();
#else
            SetUnhandledExceptionFilter(unhandledExceptionFilter);

            HRESULT hRes = CoInitializeEx(0, COINIT_MULTITHREADED);'''),
    ('''                    RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
            }''', '''                    RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
            }
#endif'''),
    ('''            controller.logMessage(String::formatted("std::exception: %s\\n", e.what()));

            CaptureAndLogCallStack();''', '''            controller.logMessage(String::formatted("std::exception: %s\\n", e.what()));

#if JUCE_WINDOWS
            CaptureAndLogCallStack();
#endif'''),
    ('''            controller.logMessage("Unknown exception\\n");

            CaptureAndLogCallStack();''', '''            controller.logMessage("Unknown exception\\n");

#if JUCE_WINDOWS
            CaptureAndLogCallStack();
#endif'''),
    ('''            // UIA holds COM references to AccessibilityNativeHandle objects for''', '''#if JUCE_WINDOWS
            // UIA holds COM references to AccessibilityNativeHandle objects for'''),
    ('''                    fn();
            }''', '''                    fn();
            }
#endif'''),
    ('''        LookAndFeel::setDefaultLookAndFeel(nullptr);

        CoUninitialize();''', '''        LookAndFeel::setDefaultLookAndFeel(nullptr);

#if JUCE_LINUX
        Logger::setCurrentLogger(nullptr);
#else
        CoUninitialize();
#endif'''),
    ('''    static constexpr int MAX_FRAMES = 64;

    static LONG WINAPI unhandledExceptionFilter''', '''    static constexpr int MAX_FRAMES = 64;

#if JUCE_LINUX
    static void linuxCrashHandler(void*)
    {
        FxLinuxLog::write("FATAL: FxSound crashed\\n" + SystemStats::getStackBacktrace());
    }

    void setWorkingDirectory()
    {
        // FX_LINUX: Factsoft/ presets and the fonts live in the data directory (the install dir on Windows)
        FxLinux::getAppDataDir().setAsCurrentWorkingDirectory();
    }
#else
    static LONG WINAPI unhandledExceptionFilter'''),
    ('''                SetCurrentDirectory(path.substr(0, pos).c_str());
            }
        }
    };''', '''                SetCurrentDirectory(path.substr(0, pos).c_str());
            }
        }
    };
#endif'''),
])

# ---------------------------------------------------------------- JUCE 7: Component::getName() returns by value
p = os.path.join(ROOT, 'GUI/FxWindow.cpp')
s = open(p, encoding='utf-8-sig').read()
s2 = s.replace('auto& text = text_button->getName();', 'auto text = text_button->getName(); // FX_LINUX: JUCE 7 returns by value')
if s2 != s:
    open(p, 'w', encoding='utf-8', newline=chr(10)).write(s2)

# ---------------------------------------------------------------- settings in ~/.config/FxSound (JUCE's Linux default is ~/FxSound)
edit('Utils/Settings/Settings.cpp', [
    ('	options.folderName = FxSound::SETTINGS_FOLDER;', '''#if JUCE_LINUX
	options.folderName = String(".config/") + FxSound::SETTINGS_FOLDER; // FX_LINUX
#else
	options.folderName = FxSound::SETTINGS_FOLDER;
#endif'''),
])
edit('GUI/FxController.cpp', [
    ('''			FxModel::getModel().pushMessage(" ", { TRANS("Click here to see what's new on this version!"), "https://www.fxsound.com/changelog" });''',
     '''#if JUCE_LINUX
			if (String(FxLinux::UPDATE_REPO).isNotEmpty())
				FxModel::getModel().pushMessage(" ", { TRANS("Click here to see what's new on this version!"), "https://github.com/" + String(FxLinux::UPDATE_REPO) + "/releases" });
#else
			FxModel::getModel().pushMessage(" ", { TRANS("Click here to see what's new on this version!"), "https://www.fxsound.com/changelog" });
#endif'''),
    ('		survey_tip_ = !settings_.getBool("survey_displayed");', '''#if JUCE_LINUX
		survey_tip_ = false; // FX_LINUX: the FxSound survey is about the Windows app
#else
		survey_tip_ = !settings_.getBool("survey_displayed");
#endif'''),
])

# ---------------------------------------------------------------- upstream bug: EQ curve not redrawn while a band is dragged
edit('GUI/FxEqualizer.cpp', [
    ('''void FxEqualizer::sliderValueChanged(Slider* slider)
{
''', '''void FxEqualizer::sliderValueChanged(Slider* slider)
{
    repaint(); // FX_LINUX fix: the curve between bands is drawn by the equalizer, not the slider
'''),
])

# ---------------------------------------------------------------- preset list order like Windows (NTFS sorts by name)
edit('GUI/FxController.cpp', [
    ('\tauto app_preset_paths = preset_search_path.findChildFiles(File::findFiles, false, "*.fac");\n',
     '\tauto app_preset_paths = preset_search_path.findChildFiles(File::findFiles, false, "*.fac");\n#if JUCE_LINUX\n\tFxLinux::sortFilesLikeWindows(app_preset_paths); // FX_LINUX\n#endif\n'),
    ('\tauto user_preset_paths = user_preset_search_path.findChildFiles(File::findFiles, false, "*.fac");\n',
     '\tauto user_preset_paths = user_preset_search_path.findChildFiles(File::findFiles, false, "*.fac");\n#if JUCE_LINUX\n\tFxLinux::sortFilesLikeWindows(user_preset_paths); // FX_LINUX\n#endif\n'),
])
edit('GUI/FxPresetImportDialog.cpp', [
    ('#include "FxPresetImportDialog.h"\n', '#include "FxPresetImportDialog.h"\n#if JUCE_LINUX\n#include "FxLinuxFeatures.h" // FX_LINUX\n#endif\n'),
    ('        auto preset_paths = preset_search_path.findChildFiles(File::findFiles, false, "*.fac");\n',
     '        auto preset_paths = preset_search_path.findChildFiles(File::findFiles, false, "*.fac");\n#if JUCE_LINUX\n        FxLinux::sortFilesLikeWindows(preset_paths); // FX_LINUX\n#endif\n'),
])

# ---------------------------------------------------------------- "Launch on system startup" toggle
# Upstream shows it only on Windows 7 (on 10/11 the installer + Task Manager own startup). Linux has
# neither, so the toggle is the only way to turn XDG autostart on/off.
edit('GUI/FxSettingsDialog.cpp', [
    ('\tif (os == SystemStats::OperatingSystemType::Windows7)\n',
     '\tif (os == SystemStats::OperatingSystemType::Windows7 || (os & SystemStats::OperatingSystemType::Linux) != 0) // FX_LINUX\n'),
])

# ---------------------------------------------------------------- upstream bug: stale EQ curve after a preset change
# Slider values set with dontSendNotification only repaint the slider itself; the curve (drawn by the
# equalizer) kept old segments, e.g. after switching presets with a hotkey.
edit('GUI/FxEqualizer.cpp', [
    ('''        auto freq = controller.getEqBandFrequency(i);
        center_frequencies_[i]->setFrequency(freq);
    }
}
''', '''        auto freq = controller.getEqBandFrequency(i);
        center_frequencies_[i]->setFrequency(freq);
    }
    repaint(); // FX_LINUX fix
}
'''),
    ('''        if (!done)
        {
            stopTimer();
        }
''', '''        repaint(); // FX_LINUX fix
        if (!done)
        {
            stopTimer();
        }
'''),
    ('''                band_boosts_[i]->setValue(band_gain_values_[i], NotificationType::dontSendNotification);
            }
        }
    }
}
''', '''                band_boosts_[i]->setValue(band_gain_values_[i], NotificationType::dontSendNotification);
            }
        }
        repaint(); // FX_LINUX fix
    }
}
'''),
])

# ---------------------------------------------------------------- per-user single instance
# JUCE's lock is /var/tmp/juceAppLock_FxSound, shared by all users (another user's FxSound - or a lock
# file owned by another user - blocks yours), and its "forward to the running instance" broadcast is a
# no-op on Linux. FxLinux::claimSingleInstance uses $XDG_RUNTIME_DIR and shows the running window.
edit('Main.cpp', [
    ('    bool moreThanOneInstanceAllowed() override       { return false; }\n',
     '#if JUCE_LINUX\n    bool moreThanOneInstanceAllowed() override       { return true; } // FX_LINUX: see FxLinux::claimSingleInstance\n#else\n    bool moreThanOneInstanceAllowed() override       { return false; }\n#endif\n'),
    ('#if JUCE_LINUX\n            SystemStats::setApplicationCrashHandler(linuxCrashHandler);\n',
     '#if JUCE_LINUX\n            if (!FxLinux::claimSingleInstance(commandline))\n                std::_Exit(0); // FX_LINUX: already running; it was asked to show its window\n            SystemStats::setApplicationCrashHandler(linuxCrashHandler);\n'),
])

# ---------------------------------------------------------------- no display -> clean exit instead of a crash
# (found on openSUSE: DISPLAY set but no X server; any JUCE window/alert then crashes in getParentMonitorArea)
edit('Main.cpp', [
    ('            FxLinux::installQuitSignalHandlers();\n',
     '            FxLinux::installQuitSignalHandlers();\n            if (!FxLinux::hasDisplay())\n                std::_Exit(1); // FX_LINUX: reason logged + printed by hasDisplay()\n'),
])

# ---------------------------------------------------------------- upstream bug: crash when quitting with a dropdown open
# ~ComboBox dismisses its open popup -> exitModalState -> mouseEnter on the half-destroyed FxView ->
# FxComboBox::highlightText on a destroyed FxComboBox (SIGSEGV). Reproduced with SIGTERM/logout while the
# preset list is open. Close popups while every component is still alive.
edit('Main.cpp', [
    ('''    void shutdown() override
    {
        if (main_window_.get() != nullptr)
        {
''', '''    void shutdown() override
    {
        PopupMenu::dismissAllActiveMenus(); // FX fix: see tools/port_gui_edits2.py
        if (auto* modal = ModalComponentManager::getInstanceWithoutCreating())
            modal->cancelAllModalComponents();
        if (main_window_.get() != nullptr)
        {
'''),
])
