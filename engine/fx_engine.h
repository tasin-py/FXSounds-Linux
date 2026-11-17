/*
 * fx_engine.h - FxSound live audio engine for Linux (PipeWire).
 *
 * Creates a virtual output device "FxSound" (a PipeWire Audio/Sink). Applications play into it,
 * the FxSound DSP processes the audio and the result is sent to the real output device.
 * This replaces the Windows audiopassthru (WASAPI loopback + virtual device) layer.
 *
 * All DSP control methods are thread-safe and may be called from the GUI thread.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class DfxDsp;
struct FxPw;

struct FxOutputDevice {
    uint32_t id = 0;
    std::string name;         // node.name (stable identifier)
    std::string description;  // human readable
    int priority = 0;
};

class FxEngine {
public:
    enum Effect { Fidelity = 0, Ambience = 1, Surround = 2, DynamicBoost = 3, Bass = 4, NumEffects = 5 };

    // By default the engine owns its DSP. The GUI passes its own DfxDsp plus the mutex that guards it,
    // so that every DSP call from the GUI thread is serialized with the audio thread.
    FxEngine(DfxDsp *external_dsp = nullptr, std::mutex *external_mutex = nullptr);
    ~FxEngine();

    // Connects to PipeWire and creates the virtual device. Returns false with a message on failure.
    bool start(std::string &error);
    // Checks that a PipeWire server is reachable, without creating anything.
    static bool probe(std::string &error);
    void stop();
    bool isRunning() const;
    // True when the PipeWire server went away; call stop() and start() again to recover.
    bool connectionLost() const;

    // ---- DSP control (same semantics as FxSound on Windows) ----
    void powerOn(bool on);
    bool isPowerOn();
    // Loads a .fac preset and applies it exactly like FxController::setPreset() does on Windows.
    bool loadPreset(const std::string &path);
    bool savePreset(const std::string &name, const std::string &path);
    std::string presetName(const std::string &path);
    float effectValue(Effect e);            // 0..1 after loadPreset, 0..10 range when set via setEffectValue
    void setEffectValue(Effect e, float v); // 0..10 (UI slider scale)
    int numEqBands();
    float eqBandFrequency(int band);
    void setEqBandFrequency(int band, float hz);
    void eqBandFrequencyRange(int band, float *lo, float *hi);
    float eqBandBoostCut(int band);
    void setEqBandBoostCut(int band, float db);
    void eqOn(bool on);
    void spectrumBandValues(float *values, int count);

    // Output silence (e.g. while the system suspends or the output device is gone).
    void mute(bool m);
    bool isMuted() const;

    // ---- Output device routing ----
    // Real output devices (excludes the FxSound device itself).
    std::vector<FxOutputDevice> outputDevices();
    // node.name of the device FxSound plays into ("" = automatic: the system's preferred device).
    std::string outputDevice();
    void setOutputDevice(const std::string &node_name);
    // Make FxSound the system default output (done automatically on start; restored on stop).
    void claimDefault(bool claim);
    bool isDefaultClaimed();
    // The real device the system would use without FxSound (the default before FxSound claimed it).
    std::string systemDefaultOutput();

    // Called (on the PipeWire thread) when the device list or the active output changes.
    void setDevicesChangedCallback(std::function<void()> cb);
    // Log sink for diagnostics ("copy log" in the GUI). Called from any thread.
    static void setLogCallback(std::function<void(const std::string &)> cb);
    static void log(const char *fmt, ...);

    // Stats
    uint32_t sampleRate() const;
    uint64_t framesProcessed() const;
    double cpuLoad() const; // fraction of the realtime budget spent in the DSP
    std::string debugStats() const;

    void notifyDevicesChanged(); // internal

private:
    friend struct FxPw;
    DfxDsp *dsp_;
    bool owns_dsp_;
    std::mutex own_mu_;
    std::mutex *dsp_mu_;
    FxPw *pw_ = nullptr;
    std::mutex cb_mu_;
    std::function<void()> devices_changed_;
    std::atomic<bool> muted_{false};
};
