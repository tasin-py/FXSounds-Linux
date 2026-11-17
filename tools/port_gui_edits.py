# Edit list for tools/port_gui.py  (edit(file, [(old, new), ...]))

# ---------------------------------------------------------------- Settings: `default` is a C++ keyword (MSVC accepted it)
for f in ['Utils/Settings/Settings.h', 'Utils/Settings/Settings.cpp']:
    p = os.path.join(ROOT, f)
    s = open(p, encoding='utf-8-sig').read()
    s2 = s.replace('bool default=false', 'bool is_default=false').replace('bool default = false', 'bool is_default = false') \
          .replace('bool default)', 'bool is_default)').replace('if (default)', 'if (is_default)')
    if s2 != s:
        open(p, 'w', encoding='utf-8', newline='\n').write(s2)
edit('Utils/Settings/Settings.cpp', [
    ('#include <Windows.h>\n#include <wincrypt.h>', '#if JUCE_WINDOWS\n#include <Windows.h>\n#include <wincrypt.h>\n#endif'),
])

# ---------------------------------------------------------------- SysInfo
edit('Utils/SysInfo/SysInfo.h', [
    ('#include <mmdeviceapi.h>\n#include <Functiondiscoverykeys_devpkey.h>',
     '#if JUCE_WINDOWS || defined(_WIN32)\n#include <mmdeviceapi.h>\n#include <Functiondiscoverykeys_devpkey.h>\n#endif'),
    ('    static bool isServiceRunning(LPCWSTR service_name);',
     '#if defined(_WIN32)\n    static bool isServiceRunning(LPCWSTR service_name);\n#endif'),
    ('private:\n\tstatic String getAudioOutputName(IMMDevice *pDevice);',
     '#if defined(_WIN32)\nprivate:\n\tstatic String getAudioOutputName(IMMDevice *pDevice);\n#endif'),
])

# ---------------------------------------------------------------- FxController.h
edit('GUI/FxController.h', [
    ('#include "AudioPassthru.h"\n#include "DfxDsp.h"\n#include <wtsapi32.h>',
     '#include "AudioPassthru.h"\n#include "DfxDsp.h"\n#if JUCE_WINDOWS\n#include <wtsapi32.h>\n#else\n#include "FxLinuxCompat.h" // FX_LINUX\n#endif'),
    ('''	void logMessage(const String& message)
	{
		file_logger_->logMessage(message);
	}''', '''	void logMessage(const String& message)
	{
#if JUCE_LINUX
		FxLinuxLog::write(message);
#else
		file_logger_->logMessage(message);
#endif
	}

	void handleHotkey(unsigned int command_id);
#if JUCE_LINUX
	// FX_LINUX additions
	AudioPassthru* getAudioPassthru() { return audio_passthru_; }
	bool isTrayAvailable();
#endif'''),
    ('''private:
	class MessageWindow
	{''', '''private:
#if JUCE_WINDOWS
	class MessageWindow
	{'''),
    ('''		LPCWSTR getClassNameFromAtom() noexcept { return (LPCWSTR)(pointer_sized_uint)atom_; }
	};
''', '''		LPCWSTR getClassNameFromAtom() noexcept { return (LPCWSTR)(pointer_sized_uint)atom_; }
	};
#endif
'''),
    ('''	static constexpr UINT CMD_NEXT_OUTPUT = 1005;

	FxController();

	static LRESULT CALLBACK eventCallback(HWND hwnd, const UINT message, const WPARAM w_param, const LPARAM l_param);''',
     '''	static constexpr UINT CMD_NEXT_OUTPUT = 1005;

	FxController();

#if JUCE_WINDOWS
	static LRESULT CALLBACK eventCallback(HWND hwnd, const UINT message, const WPARAM w_param, const LPARAM l_param);
#endif'''),
    ('''	typedef HPOWERNOTIFY(WINAPI* RegisterSuspendResumeNotificationFunc)(HANDLE, DWORD);
	typedef BOOL(WINAPI* UnregisterSuspendResumeNotificationFunc)(HPOWERNOTIFY);

	MessageWindow message_window_;
	bool hotkeys_registered_;
	HPOWERNOTIFY powerNotify_;
	UnregisterSuspendResumeNotificationFunc unregister_suspend_resume_notification_;''',
     '''#if JUCE_WINDOWS
	typedef HPOWERNOTIFY(WINAPI* RegisterSuspendResumeNotificationFunc)(HANDLE, DWORD);
	typedef BOOL(WINAPI* UnregisterSuspendResumeNotificationFunc)(HPOWERNOTIFY);

	MessageWindow message_window_;
	HPOWERNOTIFY powerNotify_;
	UnregisterSuspendResumeNotificationFunc unregister_suspend_resume_notification_;
#endif
	bool hotkeys_registered_;'''),
    ('''	AudioPassthru* audio_passthru_;
	DfxDsp dfx_dsp_;''', '''	AudioPassthru* audio_passthru_;
#if JUCE_LINUX
	FxLockedDsp dfx_dsp_; // FX_LINUX: every DSP call is serialized with the audio thread
#else
	DfxDsp dfx_dsp_;
#endif'''),
])

# ---------------------------------------------------------------- FxController.cpp
edit('GUI/FxController.cpp', [
    ('FxController::FxController() : message_window_(L"FxSoundHotkeys", (WNDPROC)eventCallback)\n{',
     '#if JUCE_WINDOWS\nFxController::FxController() : message_window_(L"FxSoundHotkeys", (WNDPROC)eventCallback)\n#else\nFxController::FxController()\n#endif\n{'),
    ('''	powerNotify_ = nullptr;
	unregister_suspend_resume_notification_ = nullptr;
''', '''#if JUCE_WINDOWS
	powerNotify_ = nullptr;
	unregister_suspend_resume_notification_ = nullptr;
#endif
'''),
    ('''	file_logger_.reset(FileLogger::createDefaultAppLogger(L"FxSound", L"fxsound.log", L"FxSound logs"));
	logMessage("v" + JUCEApplication::getInstance()->getApplicationVersion());
	logMessage(SystemStats::getOperatingSystemName());

	SYSTEM_INFO sys_info;''', '''#if JUCE_LINUX
	logMessage("FxSound for Linux v" + JUCEApplication::getInstance()->getApplicationVersion() + " starting");
	logMessage(SystemStats::getOperatingSystemName() + ", " + SystemStats::getCpuModel());
#else
	file_logger_.reset(FileLogger::createDefaultAppLogger(L"FxSound", L"fxsound.log", L"FxSound logs"));
	logMessage("v" + JUCEApplication::getInstance()->getApplicationVersion());
	logMessage(SystemStats::getOperatingSystemName());

	SYSTEM_INFO sys_info;'''),
    ('''		logMessage(String("ARM64"));
	}
''', '''		logMessage(String("ARM64"));
	}
#endif
'''),
    ('''	SetWindowLongPtr(message_window_.getHandle(), GWLP_USERDATA, (LONG_PTR)this);

	session_id_ = 0;
	ProcessIdToSessionId(GetCurrentProcessId(), &session_id_);
	WTSRegisterSessionNotification(message_window_.getHandle(), NOTIFY_FOR_THIS_SESSION);
}''', '''#if JUCE_WINDOWS
	SetWindowLongPtr(message_window_.getHandle(), GWLP_USERDATA, (LONG_PTR)this);

	session_id_ = 0;
	ProcessIdToSessionId(GetCurrentProcessId(), &session_id_);
	WTSRegisterSessionNotification(message_window_.getHandle(), NOTIFY_FOR_THIS_SESSION);
#endif
}'''),
    ('''	if (powerNotify_ != nullptr && unregister_suspend_resume_notification_ != nullptr)
	{
		unregister_suspend_resume_notification_(powerNotify_);
		powerNotify_ = nullptr;
	}
	stopTimer();
	WTSUnRegisterSessionNotification(message_window_.getHandle());
	unregisterHotkeys();''', '''#if JUCE_WINDOWS
	if (powerNotify_ != nullptr && unregister_suspend_resume_notification_ != nullptr)
	{
		unregister_suspend_resume_notification_(powerNotify_);
		powerNotify_ = nullptr;
	}
#endif
	stopTimer();
#if JUCE_WINDOWS
	WTSUnRegisterSessionNotification(message_window_.getHandle());
#endif
	unregisterHotkeys();'''),
    ('''	if (AttachConsole(ATTACH_PARENT_PROCESS))
	{''', '''#if JUCE_LINUX
	std::cout << json_text << std::endl;
#else
	if (AttachConsole(ATTACH_PARENT_PROCESS))
	{'''),
    ('''		FreeConsole();
	}
}''', '''		FreeConsole();
	}
#endif
}'''),
    ('''			String message(TRANS("Error in system audio configuration. Unable to run FxSound"));''',
     '''#if JUCE_LINUX
			String message(TRANS("Error in system audio configuration. Unable to run FxSound") + "\\n\\n" + String(audio_passthru_->lastError()));
#else
			String message(TRANS("Error in system audio configuration. Unable to run FxSound"));
#endif'''),
    ('''			RegDeleteTree(HKEY_CURRENT_USER, L"Software\\\\DFX");''',
     '''#if JUCE_WINDOWS
			RegDeleteTree(HKEY_CURRENT_USER, L"Software\\\\DFX");
#endif'''),
    ('''		HMODULE user32_module = GetModuleHandleW(L"user32.dll");''', '''#if JUCE_WINDOWS
		HMODULE user32_module = GetModuleHandleW(L"user32.dll");'''),
    ('''				powerNotify_ = register_suspend_resume_notification(message_window_.getHandle(), DEVICE_NOTIFY_WINDOW_HANDLE);
			}
		}''', '''				powerNotify_ = register_suspend_resume_notification(message_window_.getHandle(), DEVICE_NOTIFY_WINDOW_HANDLE);
			}
		}
#endif'''),
    ('L"FxSound\\\\Presets");\n\t\tif (!path.exists())', 'L"FxSound/Presets");\n\t\tif (!path.exists())'),
    ('return data_dir + L"FxSound\\\\AutoSave";', 'return data_dir + L"FxSound/AutoSave";'),
    ('return getAutoSavePath() + L"\\\\" + preset_name + ".fac";', 'return getAutoSavePath() + L"/" + preset_name + ".fac";'),
    ('FileSearchPath user_preset_search_path(data_dir + L"FxSound\\\\Presets");', 'FileSearchPath user_preset_search_path(data_dir + L"FxSound/Presets");'),
    ('''		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\\\Presets";
		dfx_dsp_.savePreset(preset.name.toWideCharPointer(), path.toWideCharPointer());''',
     '''		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound/Presets";
		File(path).createDirectory();
		dfx_dsp_.savePreset(preset.name.toWideCharPointer(), path.toWideCharPointer());'''),
    ('''		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\\\Presets";
		dfx_dsp_.savePreset(preset_name.toWideCharPointer(), path.toWideCharPointer());''',
     '''		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound/Presets";
		File(path).createDirectory();
		dfx_dsp_.savePreset(preset_name.toWideCharPointer(), path.toWideCharPointer());'''),
    ('''		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\\\Presets";
		dfx_dsp_.savePreset(new_name.toWideCharPointer(), path.toWideCharPointer());

		wchar_t old_path[MAX_PATH] = {};''', '''		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound/Presets";
		dfx_dsp_.savePreset(new_name.toWideCharPointer(), path.toWideCharPointer());

#if JUCE_LINUX
		if (File(preset.path) != File(path).getChildFile(new_name + ".fac"))
			File(preset.path).deleteFile();
#else
		wchar_t old_path[MAX_PATH] = {};'''),
    ('''		SHFileOperation(&file_op);

		initPresets();

		setPreset(new_name);''', '''		SHFileOperation(&file_op);
#endif

		initPresets();

		setPreset(new_name);'''),
    ('''		deleteAutoSavedPreset(preset.name);

		wchar_t path[MAX_PATH] = {};''', '''		deleteAutoSavedPreset(preset.name);

#if JUCE_LINUX
		File(preset.path).deleteFile();
#else
		wchar_t path[MAX_PATH] = {};'''),
    ('''		SHFileOperation(&file_op);

		initPresets();

		auto device_config''', '''		SHFileOperation(&file_op);
#endif

		initPresets();

		auto device_config'''),
    ('''		if (preset.type == FxModel::PresetType::UserPreset)
		{
			wchar_t path[MAX_PATH] = {};''', '''		if (preset.type == FxModel::PresetType::UserPreset)
		{
#if JUCE_LINUX
			File(preset.path).deleteFile();
#else
			wchar_t path[MAX_PATH] = {};'''),
    ('''			SHFileOperation(&file_op);
		}
	}

	initPresets();''', '''			SHFileOperation(&file_op);
#endif
		}
	}

	initPresets();'''),
    ('getFullPathName()) + L"FxSound\\\\Presets\\\\Export\\\\";', 'getFullPathName()) + L"FxSound/Presets/Export/";'),
    ('''	auto path_name = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\\\Presets";
	File path(path_name);''', '''	auto path_name = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound/Presets";
	File path(path_name);'''),
    ('File import_file(path_name + "\\\\" + preset_info.name.c_str() + ".fac");', 'File import_file(path_name + "/" + preset_info.name.c_str() + ".fac");'),
    # hotkey handling: factor WM_HOTKEY body into handleHotkey() (shared with the Linux X11 hotkeys)
    ('''LRESULT CALLBACK FxController::eventCallback(HWND hwnd, const UINT message, const WPARAM w_param, const LPARAM l_param)
{
	static auto os = SystemStats::getOperatingSystemType();
	FxController* controller = (FxController*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_HOTKEY:
	{''', '''void FxController::handleHotkey(unsigned int w_param)
{
	FxController* controller = this;
	{''', 'void FxController::handleHotkey'),
    ('''					controller->setOutput(output_index);
					break;
				}
			}
		}
	}
	break;

	case WM_POWERBROADCAST:''', '''					controller->setOutput(output_index);
					break;
				}
			}
		}
	}
}
#if JUCE_WINDOWS
LRESULT CALLBACK FxController::eventCallback(HWND hwnd, const UINT message, const WPARAM w_param, const LPARAM l_param)
{
	static auto os = SystemStats::getOperatingSystemType();
	FxController* controller = (FxController*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_HOTKEY:
	{
		FX_WINDOWS_HOTKEY_BODY
	}
	break;

	case WM_POWERBROADCAST:''', 'handleHotkey((unsigned int)w_param)'),
    ('''	return DefWindowProc(hwnd, message, w_param, l_param);
}''', '''	return DefWindowProc(hwnd, message, w_param, l_param);
}
#endif'''),
    ('''	if (session_id_ != WTSGetActiveConsoleSessionId())
		return; // Ignore device changes on another user session''', '''#if JUCE_WINDOWS
	if (session_id_ != WTSGetActiveConsoleSessionId())
		return; // Ignore device changes on another user session
#endif'''),
    ('''			ChildProcess child_process;
			child_process.start("updater.exe /silent");''', '''#if JUCE_LINUX
			FxLinux::checkForUpdates(false);
#else
			ChildProcess child_process;
			child_process.start("updater.exe /silent");
#endif'''),
    ('''bool FxController::isLaunchOnStartup()
{
	HKEY hkey;''', '''bool FxController::isLaunchOnStartup()
{
#if JUCE_LINUX
	return FxLinux::isAutostartEnabled();
#else
	HKEY hkey;'''),
    ('''	return size > 0;
}''', '''	return size > 0;
#endif
}'''),
    ('''void FxController::setLaunchOnStartup(bool launch_on_startup)
{
	wchar_t szPath[MAX_PATH];''', '''void FxController::setLaunchOnStartup(bool launch_on_startup)
{
#if JUCE_LINUX
	FxLinux::setAutostartEnabled(launch_on_startup);
#else
	wchar_t szPath[MAX_PATH];'''),
    ('''		RegDeleteValue(hkey, L"FxSound");
	}

	RegCloseKey(hkey);
}''', '''		RegDeleteValue(hkey, L"FxSound");
	}

	RegCloseKey(hkey);
#endif
}'''),
    # hotkey registration -> X11 on Linux
    ('''	if (hk_id != 0)
	{
		::UnregisterHotKey(message_window_.getHandle(), hk_id);''', '''	if (hk_id != 0)
	{
#if JUCE_LINUX
		FxLinux::Hotkeys::unregisterHotkey(hk_id);
		if (code == 0)
		{
			settings_.setInt(command, code);
			return false;
		}
		if (FxLinux::Hotkeys::registerHotkey(hk_id, new_mod, new_vk))
		{
			settings_.setInt(command, code);
			return true;
		}
		settings_.setInt(command, 0);
		return false;
#else
		::UnregisterHotKey(message_window_.getHandle(), hk_id);'''),
    ('''		else
		{
			settings_.setInt(command, 0);
			return false;
		}
	}

	return false;
}''', '''		else
		{
			settings_.setInt(command, 0);
			return false;
		}
#endif
	}

	return false;
}'''),
    ('''	HKL hkl = GetKeyboardLayout(0);''', '''#if JUCE_LINUX
	return true; // FX_LINUX: Ctrl+Alt / Ctrl+Shift + letter/digit never types a character
#else
	HKL hkl = GetKeyboardLayout(0);'''),
    ('''	if (len > 0 && output[0] >= 0x20) // The hotkey combination generates a non-control character
	{
		return false;
	}

	return true;
}''', '''	if (len > 0 && output[0] >= 0x20) // The hotkey combination generates a non-control character
	{
		return false;
	}

	return true;
#endif
}'''),
    ('''				::RegisterHotKey(message_window_.getHandle(), CMD_ON_OFF, mod, vk);''', '''				FX_REGISTER_HOTKEY(CMD_ON_OFF, mod, vk);'''),
    ('''				::RegisterHotKey(message_window_.getHandle(), CMD_OPEN_CLOSE, mod, vk);''', '''				FX_REGISTER_HOTKEY(CMD_OPEN_CLOSE, mod, vk);'''),
    ('''				::RegisterHotKey(message_window_.getHandle(), CMD_NEXT_PRESET, mod, vk);''', '''				FX_REGISTER_HOTKEY(CMD_NEXT_PRESET, mod, vk);'''),
    ('''				::RegisterHotKey(message_window_.getHandle(), CMD_PREVIOUS_PRESET, mod, vk);''', '''				FX_REGISTER_HOTKEY(CMD_PREVIOUS_PRESET, mod, vk);'''),
    ('''				::RegisterHotKey(message_window_.getHandle(), CMD_NEXT_OUTPUT, mod, vk);''', '''				FX_REGISTER_HOTKEY(CMD_NEXT_OUTPUT, mod, vk);'''),
    ('''		::UnregisterHotKey(message_window_.getHandle(), CMD_ON_OFF);
		::UnregisterHotKey(message_window_.getHandle(), CMD_OPEN_CLOSE);
		::UnregisterHotKey(message_window_.getHandle(), CMD_NEXT_PRESET);
		::UnregisterHotKey(message_window_.getHandle(), CMD_PREVIOUS_PRESET);
		::UnregisterHotKey(message_window_.getHandle(), CMD_NEXT_OUTPUT);''', '''		FX_UNREGISTER_HOTKEY(CMD_ON_OFF);
		FX_UNREGISTER_HOTKEY(CMD_OPEN_CLOSE);
		FX_UNREGISTER_HOTKEY(CMD_NEXT_PRESET);
		FX_UNREGISTER_HOTKEY(CMD_PREVIOUS_PRESET);
		FX_UNREGISTER_HOTKEY(CMD_NEXT_OUTPUT);'''),
    ('''String FxController::FormatString(const String& format, const String& arg)
{
	wchar_t buffer[1024];''', '''#if JUCE_WINDOWS
#define FX_REGISTER_HOTKEY_DEFINED
#endif
String FxController::FormatString(const String& format, const String& arg)
{
#if JUCE_LINUX
	return format.replace("%s", arg); // FX_LINUX: the translations only use a single %s
#endif
	wchar_t buffer[1024];'''),
])

# The Windows WM_HOTKEY handler body was moved into handleHotkey(); on Windows keep it by
# calling the same code through a macro so the Windows build stays identical in behaviour.
p = os.path.join(ROOT, 'GUI/FxController.cpp')
s = open(p, encoding='utf-8').read()
if 'FX_WINDOWS_HOTKEY_BODY\n' in s and '#define FX_WINDOWS_HOTKEY_BODY' not in s:
    s = s.replace('#include <cstdio>\n', '''#include <cstdio>
#if JUCE_WINDOWS
#define FX_REGISTER_HOTKEY(id, mod, vk) ::RegisterHotKey(message_window_.getHandle(), id, mod, vk)
#define FX_UNREGISTER_HOTKEY(id) ::UnregisterHotKey(message_window_.getHandle(), id)
#else
#include "FxLinuxFeatures.h" // FX_LINUX
#define FX_REGISTER_HOTKEY(id, mod, vk) FxLinux::Hotkeys::registerHotkey(id, mod, vk)
#define FX_UNREGISTER_HOTKEY(id) FxLinux::Hotkeys::unregisterHotkey(id)
#endif
''', 1)
    s = s.replace('FX_WINDOWS_HOTKEY_BODY', 'controller->handleHotkey((unsigned int)w_param);')
    open(p, 'w', encoding='utf-8', newline='\n').write(s)
