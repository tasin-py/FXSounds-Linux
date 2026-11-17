/*
 * AudioPassthruLinux.cpp - Linux implementation of the GUI's AudioPassthru interface on PipeWire.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include <JuceHeader.h>
#include "AudioPassthru.h"
#include "FxLockedDsp.h"
#include "fx_engine.h"
#include "FxLinuxLog.h"

#include <atomic>
#include <memory>

static std::wstring W(const std::string &s) { return juce::String::fromUTF8(s.c_str()).toWideCharPointer(); }
static std::string N(const std::wstring &s) { return juce::String(s.c_str()).toStdString(); }

static const wchar_t *FX_DEVICE_NAME = L"FxSound Audio Enhancer";

class AudioPassthruPrivate
{
public:
    std::unique_ptr<FxEngine> engine;
    FxLockedDsp *dsp = nullptr;
    AudioPassthruCallback *callback = nullptr;
    std::atomic<bool> change_pending{false};
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    bool claimed = false;
    bool muted = false;
    std::string error;
    juce::int64 last_restart_ms = 0;

    void deviceChangedFromPipeWire()
    {
        // called on the PipeWire thread: hop to the message thread, coalescing bursts
        if (change_pending.exchange(true)) return;
        auto flag = alive;
        juce::MessageManager::callAsync([this, flag]() {
            if (!flag->load()) return;
            change_pending = false;
            if (callback) callback->onSoundDeviceChange(true);
        });
    }

    bool startEngine()
    {
        engine = std::make_unique<FxEngine>(dsp->raw(), dsp->mutex());
        engine->setDevicesChangedCallback([this]() { deviceChangedFromPipeWire(); });
        engine->mute(muted);
        if (!engine->start(error)) {
            FxLinuxLog::write("engine start failed: " + juce::String(error));
            engine.reset();
            return false;
        }
        // start() claims the default; the controller decides via processTimer/restoreDefault.
        engine->claimDefault(claimed);
        return true;
    }
};

AudioPassthru::AudioPassthru() : data_(new AudioPassthruPrivate()) {}

AudioPassthru::~AudioPassthru()
{
    data_->alive->store(false);
    if (data_->engine) data_->engine->stop();
    delete data_;
}

int AudioPassthru::init()
{
    FxEngine::setLogCallback([](const std::string &m) { FxLinuxLog::write(juce::String::fromUTF8(m.c_str())); });
    if (!FxEngine::probe(data_->error)) {
        FxLinuxLog::write(juce::String::fromUTF8(data_->error.c_str()));
        return -1;
    }
    return 0;
}

void AudioPassthru::setDspProcessingModule(FxLockedDsp *dsp)
{
    data_->dsp = dsp;
    data_->claimed = true;
    data_->startEngine();
}

void AudioPassthru::mute(bool m)
{
    data_->muted = m;
    if (data_->engine) data_->engine->mute(m);
}

std::vector<SoundDevice> AudioPassthru::getSoundDevices(bool)
{
    std::vector<SoundDevice> list;
    if (!data_->engine) return list;
    auto active = data_->engine->outputDevice();
    auto sys_default = data_->engine->systemDefaultOutput();
    for (auto &d : data_->engine->outputDevices()) {
        SoundDevice s;
        s.isPlaybackDevice = true;
        s.isRealDevice = true;
        s.isActive = true;
        s.pwszID = W(d.name);
        s.pwszIDRealDevices = s.pwszID;
        s.deviceFriendlyName = W(d.description);
        s.deviceDescription = s.deviceFriendlyName;
        s.deviceFriendlyNameRealDevices = s.deviceFriendlyName;
        s.deviceDescriptionRealDevices = s.deviceFriendlyName;
        s.isTargetedRealPlaybackDevice = d.name == active;
        s.isUserSelectedPlaybackDevice = s.isTargetedRealPlaybackDevice;
        s.isDefaultDevice = d.name == sys_default;
        s.deviceNumChannel = 2;
        list.push_back(s);
    }
    SoundDevice fx;
    fx.isPlaybackDevice = true;
    fx.isDFXDevice = true;
    fx.isActive = true;
    fx.pwszID = L"fxsound";
    fx.deviceFriendlyName = FX_DEVICE_NAME;
    fx.deviceDescription = L"FxSound";
    list.push_back(fx);
    return list;
}

int AudioPassthru::setBufferLength(int) { return 0; }

int AudioPassthru::processTimer()
{
    auto &d = *data_;
    if (!d.engine || d.engine->connectionLost()) {
        // PipeWire restarted (update, logout of another session...) or was not running: reconnect.
        auto now = juce::Time::currentTimeMillis();
        if (now - d.last_restart_ms > 2000 && d.dsp) {
            d.last_restart_ms = now;
            if (d.engine) { FxLinuxLog::write("PipeWire connection lost, reconnecting"); d.engine->stop(); }
            if (d.startEngine()) d.deviceChangedFromPipeWire();
        }
        return 0;
    }
    if (!d.claimed) {
        d.claimed = true;
        d.engine->claimDefault(true);
    }
    return 0;
}

void AudioPassthru::setAsPlaybackDevice(const SoundDevice sound_device)
{
    if (data_->engine) data_->engine->setOutputDevice(N(sound_device.pwszID));
}

void AudioPassthru::registerCallback(AudioPassthruCallback *callback) { data_->callback = callback; }

bool AudioPassthru::isPlaybackDeviceAvailable()
{
    return data_->engine && !data_->engine->outputDevice().empty();
}

bool AudioPassthru::checkDeviceChanges()
{
    data_->deviceChangedFromPipeWire();
    return true;
}

void AudioPassthru::restoreDefaultPlaybackDevice()
{
    data_->claimed = false;
    if (data_->engine) data_->engine->claimDefault(false);
}

std::string AudioPassthru::lastError() const { return data_->error; }

std::string AudioPassthru::diagnostics()
{
    juce::String s;
    if (!data_->engine) return "engine: not running (" + data_->error + ")\n";
    auto &e = *data_->engine;
    s << "engine: running, rate " << (int)e.sampleRate() << " Hz, frames " << (juce::int64)e.framesProcessed()
      << ", dsp load " << juce::String(e.cpuLoad() * 100.0, 2) << "%\n";
    s << "default claimed: " << (e.isDefaultClaimed() ? "yes" : "no") << ", muted: " << (e.isMuted() ? "yes" : "no")
      << ", connection lost: " << (e.connectionLost() ? "yes" : "no") << "\n";
    s << "output: " << e.outputDevice() << "\nsystem default (without FxSound): " << e.systemDefaultOutput() << "\n";
    s << "devices:\n";
    for (auto &d : e.outputDevices()) s << "  " << d.name << "  [" << d.description << "] prio " << d.priority << "\n";
    s << e.debugStats() << "\n";
    return s.toStdString();
}
