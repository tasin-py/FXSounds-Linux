/*
 * FxLockedDsp.h - thread-safe facade over DfxDsp for the Linux GUI.
 *
 * On Windows the GUI thread and the audio thread call DfxDsp concurrently without a lock.
 * On Linux every call is serialized with the PipeWire audio thread through one mutex, so preset
 * loads and slider moves can never race with processing (no clicks, no torn EQ state).
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#pragma once
#include "DfxDsp.h"
#include <mutex>
#include <string>

class FxLockedDsp
{
public:
    using Effect = DfxDsp::Effect;

    DfxDsp *raw() { return &dsp_; }
    std::mutex *mutex() { return &mu_; }

#define FX_LOCK std::lock_guard<std::mutex> l_(mu_)
    int loadPreset(std::wstring p) { FX_LOCK; return dsp_.loadPreset(p); }
    int savePreset(std::wstring n, std::wstring p) { FX_LOCK; return dsp_.savePreset(n, p); }
    int exportPreset(std::wstring s, std::wstring n, std::wstring p) { FX_LOCK; return dsp_.exportPreset(s, n, p); }
    DfxPreset getPresetInfo(std::wstring p) { FX_LOCK; return dsp_.getPresetInfo(p); }
    void eqOn(bool on) { FX_LOCK; dsp_.eqOn(on); }
    int getNumEqBands() { FX_LOCK; return dsp_.getNumEqBands(); }
    float getBalance() { FX_LOCK; return dsp_.getBalance(); }
    void setBalance(float v) { FX_LOCK; dsp_.setBalance(v); }
    float getNormalization() { FX_LOCK; return dsp_.getNormalization(); }
    void setNormalization(float v) { FX_LOCK; dsp_.setNormalization(v); }
    float getVolumeLeveling() { FX_LOCK; return dsp_.getVolumeLeveling(); }
    void setVolumeLeveling(float v) { FX_LOCK; dsp_.setVolumeLeveling(v); }
    float getMasterGain() { FX_LOCK; return dsp_.getMasterGain(); }
    void setMasterGain(float v) { FX_LOCK; dsp_.setMasterGain(v); }
    float getFilterQ() { FX_LOCK; return dsp_.getFilterQ(); }
    void setFilterQ(float v) { FX_LOCK; dsp_.setFilterQ(v); }
    void setNumBands(int n) { FX_LOCK; dsp_.setNumBands(n); }
    float getEqBandFrequency(int b) { FX_LOCK; return dsp_.getEqBandFrequency(b); }
    void setEqBandFrequency(int b, float f) { FX_LOCK; dsp_.setEqBandFrequency(b, f); }
    void getEqBandFrequencyRange(int b, float *lo, float *hi) { FX_LOCK; dsp_.getEqBandFrequencyRange(b, lo, hi); }
    float getEqBandBoostCut(int b) { FX_LOCK; return dsp_.getEqBandBoostCut(b); }
    void setEqBandBoostCut(int b, float v) { FX_LOCK; dsp_.setEqBandBoostCut(b, v); }
    void powerOn(bool on) { FX_LOCK; dsp_.powerOn(on); }
    bool isPowerOn() { FX_LOCK; return dsp_.isPowerOn(); }
    float getEffectValue(Effect e) { FX_LOCK; return dsp_.getEffectValue(e); }
    void setEffectValue(Effect e, float v) { FX_LOCK; dsp_.setEffectValue(e, v); }
    unsigned long getTotalAudioProcessedTime() { FX_LOCK; return dsp_.getTotalAudioProcessedTime(); }
    void resetTotalAudioProcessedTime() { FX_LOCK; dsp_.resetTotalAudioProcessedTime(); }
    void getSpectrumBandValues(float *v, int n) { FX_LOCK; dsp_.getSpectrumBandValues(v, n); }
#undef FX_LOCK

private:
    std::mutex mu_;
    DfxDsp dsp_;
};
