/*
 * AudioPassthru.h (Linux) - same interface the FxSound GUI uses on Windows, implemented on the
 * PipeWire engine (engine/fx_engine.h) instead of WASAPI + the FxSound virtual driver.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#ifndef _AUDIOPASSTHRU_H_
#define _AUDIOPASSTHRU_H_

#include <string>
#include <vector>
#include <cwchar>

class FxLockedDsp;

struct SoundDevice {
	void *pAllDevices = nullptr;
	bool isCaptureDevice = false;
	bool isPlaybackDevice = false;
	bool isTargetedRealPlaybackDevice = false;
	bool isRealDevice = false;
	bool isDFXDevice = false;
	bool isUserSelectedPlaybackDevice = false;
	bool isDefaultDevice = false;
	bool isActive = false;

	std::wstring pwszID;              // PipeWire node.name of the device
	std::wstring pwszIDRealDevices;
	wchar_t pwszIDPreviousRealDevices[512] = {};

	std::wstring deviceFriendlyName;  // node.description, e.g. "Built-in Audio Analog Stereo"
	std::wstring deviceDescription;
	std::wstring deviceFriendlyNameRealDevices;
	std::wstring deviceDescriptionRealDevices;
	std::wstring deviceFormFactor;
	int deviceNumChannel = 2;
};

class AudioPassthruCallback
{
public:
	virtual void onSoundDeviceChange(bool processing) = 0;
};

class AudioPassthruPrivate;
class AudioPassthru
{
public:
	AudioPassthru();
	~AudioPassthru();
	int init();
	void mute(bool mute);
	std::vector<SoundDevice> getSoundDevices(bool active_devices = true);
	int setBufferLength(int i_buffer_length_msecs);
	int processTimer();
	void setDspProcessingModule(FxLockedDsp* pDspProcessingModule);
	void setAsPlaybackDevice(const SoundDevice sound_device);
	void registerCallback(AudioPassthruCallback *callback);
	bool isPlaybackDeviceAvailable();
	bool checkDeviceChanges();
	void restoreDefaultPlaybackDevice();

	// Linux additions
	std::string lastError() const;
	std::string diagnostics();   // engine/device state for "Copy log"

private:
	AudioPassthruPrivate *data_;
};

#endif
