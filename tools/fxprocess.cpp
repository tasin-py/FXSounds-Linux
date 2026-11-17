/*
 * fxprocess - run a WAV file through the FxSound DSP engine on Linux.
 *
 *   fxprocess in.wav out.wav [options]
 *     --preset FILE.fac     load an FxSound preset (same files as the Windows app)
 *     --fidelity N  --ambience N  --surround N  --dynamic N  --bass N   (0..10, like the UI sliders)
 *     --eq BAND=DB          set graphic EQ band boost/cut (repeatable)
 *     --off                 power off (bypass) - output should equal input
 *     --info                print the preset/effect/EQ state and exit
 *     --block N             frames per processing call (default 480 = 10 ms @ 48 kHz)
 *
 * Output is 32-bit float WAV (the engine works in float, as on Windows).
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "DfxDsp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>

static std::wstring widen(const std::string &s)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(s);
}
static std::string narrow(const std::wstring &s)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(s);
}

struct Wav { int channels = 0, rate = 0; std::vector<float> samples; };

static bool read_wav(const char *path, Wav &w)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return false; }
    char riff[12];
    if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) || memcmp(riff + 8, "WAVE", 4)) {
        fprintf(stderr, "%s: not a RIFF/WAVE file\n", path); fclose(f); return false;
    }
    int fmt_tag = 0, bits = 0;
    bool have_fmt = false;
    for (;;) {
        char id[4]; uint32_t size;
        if (fread(id, 1, 4, f) != 4 || fread(&size, 4, 1, f) != 1) break;
        if (!memcmp(id, "fmt ", 4)) {
            std::vector<uint8_t> b(size);
            if (fread(b.data(), 1, size, f) != size) break;
            fmt_tag = b[0] | (b[1] << 8);
            w.channels = b[2] | (b[3] << 8);
            w.rate = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24);
            bits = b[14] | (b[15] << 8);
            if (fmt_tag == 0xFFFE && size >= 26) fmt_tag = b[24] | (b[25] << 8); // WAVE_FORMAT_EXTENSIBLE subformat
            have_fmt = true;
        } else if (!memcmp(id, "data", 4) && have_fmt) {
            std::vector<uint8_t> d(size);
            size_t got = fread(d.data(), 1, size, f);
            size_t bps = bits / 8, n = got / bps;
            w.samples.resize(n);
            for (size_t i = 0; i < n; i++) {
                const uint8_t *p = &d[i * bps];
                float v;
                if (fmt_tag == 3 && bits == 32) memcpy(&v, p, 4);
                else if (bits == 16) v = (int16_t)(p[0] | (p[1] << 8)) / 32768.0f;
                else if (bits == 24) v = (int32_t)((p[0] << 8) | (p[1] << 16) | (p[2] << 24)) / 2147483648.0f;
                else if (bits == 32) v = (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)) / 2147483648.0f;
                else { fprintf(stderr, "unsupported WAV format (tag %d, %d bits)\n", fmt_tag, bits); fclose(f); return false; }
                w.samples[i] = v;
            }
            fclose(f);
            return true;
        } else {
            fseek(f, size + (size & 1), SEEK_CUR);
        }
    }
    fprintf(stderr, "%s: no audio data found\n", path);
    fclose(f);
    return false;
}

static bool write_wav_float(const char *path, const Wav &w)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return false; }
    uint32_t data = (uint32_t)(w.samples.size() * 4);
    uint32_t riff = 36 + data, fmtsz = 16, rate = w.rate, br = w.rate * w.channels * 4;
    uint16_t tag = 3, ch = (uint16_t)w.channels, align = (uint16_t)(w.channels * 4), bits = 32;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&fmtsz, 4, 1, f); fwrite(&tag, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(w.samples.data(), 4, w.samples.size(), f);
    fclose(f);
    return true;
}

static void print_state(DfxDsp &dsp)
{
    const char *names[] = {"Fidelity", "Ambience", "Surround", "Dynamic Boost", "Bass"};
    printf("power: %s\n", dsp.isPowerOn() ? "on" : "off");
    for (int e = 0; e < DfxDsp::NumEffects; e++)
        printf("  %-14s %.2f\n", names[e], dsp.getEffectValue((DfxDsp::Effect)e));
    int nb = dsp.getNumEqBands();
    printf("EQ bands: %d\n", nb);
    for (int b = 0; b < nb; b++)
        printf("  band %2d  %8.1f Hz  %+6.2f dB\n", b, dsp.getEqBandFrequency(b), dsp.getEqBandBoostCut(b));
    printf("master gain %.2f, balance %.2f, volume leveling %.2f, normalization %.2f, filter Q %.2f\n",
           dsp.getMasterGain(), dsp.getBalance(), dsp.getVolumeLeveling(), dsp.getNormalization(), dsp.getFilterQ());
}

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#pragma comment(lib, "advapi32.lib")
// The DSP reads and WRITES its session state under HKCU\Software\DFX. When this tool runs on Windows
// (null-test reference) we must not touch the user's real FxSound settings: redirect HKCU to an empty
// sandbox key that is wiped on every run. Must run before DfxDsp is constructed.
static struct RegistrySandbox {
    RegistrySandbox()
    {
        const wchar_t *path = L"Software\\FxSoundLinuxPort\\TestSandboxHKCU";
        RegDeleteTreeW(HKEY_CURRENT_USER, path);
        HKEY k;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &k, nullptr) == ERROR_SUCCESS) {
            if (RegOverridePredefKey(HKEY_CURRENT_USER, k) != ERROR_SUCCESS) { fprintf(stderr, "registry sandbox failed\n"); exit(3); }
            RegCloseKey(k);
        } else { fprintf(stderr, "registry sandbox failed\n"); exit(3); }
    }
} g_registry_sandbox;
#endif

int main(int argc, char **argv)
{
    std::setlocale(LC_ALL, "C.UTF-8");
    if (argc < 2) {
        fprintf(stderr, "usage: %s in.wav out.wav [--preset FILE.fac] [--fidelity N ...] [--eq BAND=DB] [--off] [--info]\n", argv[0]);
        return 2;
    }

    DfxDsp dsp;
    const char *in = nullptr, *out = nullptr, *preset = nullptr;
    bool info = false, off = false, gui_init = false;
    int block = 480;
    float effects[DfxDsp::NumEffects]; bool set_effect[DfxDsp::NumEffects] = {};
    std::vector<std::pair<int, float>> eq;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> const char * { if (i + 1 >= argc) { fprintf(stderr, "%s needs a value\n", a.c_str()); exit(2); } return argv[++i]; };
        if (a == "--preset") preset = next();
        else if (a == "--info") info = true;
        else if (a == "--off") off = true;
        else if (a == "--gui-init") gui_init = true;
        else if (a == "--block") block = atoi(next());
        else if (a == "--fidelity") { effects[0] = atof(next()); set_effect[0] = true; }
        else if (a == "--ambience") { effects[1] = atof(next()); set_effect[1] = true; }
        else if (a == "--surround") { effects[2] = atof(next()); set_effect[2] = true; }
        else if (a == "--dynamic")  { effects[3] = atof(next()); set_effect[3] = true; }
        else if (a == "--bass")     { effects[4] = atof(next()); set_effect[4] = true; }
        else if (a == "--eq") { std::string v = next(); auto eqp = v.find('='); eq.push_back({atoi(v.substr(0, eqp).c_str()), (float)atof(v.substr(eqp + 1).c_str())}); }
        else if (!in) in = argv[i];
        else if (!out) out = argv[i];
        else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; }
    }

    if (gui_init) {
        // Same as FxController::initConfig() with default settings (Windows GUI start-up)
        dsp.setNumBands(10);
        dsp.setVolumeLeveling(0.0f);
        dsp.setBalance(0.0f);
        dsp.setFilterQ(1.0f);
        dsp.setMasterGain(0.0f);
    }
    if (preset) {
        DfxPreset p = dsp.getPresetInfo(widen(preset));
        if (dsp.loadPreset(widen(preset)) != 0) { fprintf(stderr, "failed to load preset %s\n", preset); return 1; }
        printf("preset: %s\n", narrow(p.name).c_str());
        // Same sequence as FxController::setPreset() in the Windows app
        for (int e = 0; e < DfxDsp::NumEffects; e++)
            dsp.setEffectValue((DfxDsp::Effect)e, dsp.getEffectValue((DfxDsp::Effect)e) * 10);
        for (int b = 0; b < dsp.getNumEqBands(); b++) {
            dsp.setEqBandFrequency(b, dsp.getEqBandFrequency(b));
            dsp.setEqBandBoostCut(b, dsp.getEqBandBoostCut(b));
        }
    }
    for (int e = 0; e < DfxDsp::NumEffects; e++)
        if (set_effect[e]) dsp.setEffectValue((DfxDsp::Effect)e, effects[e]);
    for (auto &b : eq) dsp.setEqBandBoostCut(b.first, b.second);
    dsp.powerOn(!off);

    if (info || !out) { print_state(dsp); return 0; }

    Wav w;
    if (!read_wav(in, w)) return 1;
    printf("input: %d Hz, %d ch, %.2f s\n", w.rate, w.channels, w.samples.size() / (double)w.channels / w.rate);

    if (dsp.setSignalFormat(32, w.channels, w.rate, 32) != 0)
        fprintf(stderr, "note: setSignalFormat returned NOT_OKAY on first call (same as Windows)\n");

    Wav o = w;
    size_t frames = w.samples.size() / w.channels;
    for (size_t pos = 0; pos < frames; pos += block) {
        int n = (int)std::min<size_t>(block, frames - pos);
        float *p = &o.samples[pos * w.channels];
        dsp.setSignalFormat(32, w.channels, w.rate, 32);
        dsp.processAudio((short int *)p, (short int *)p, n, 0);
    }

    double peak_in = 0, peak_out = 0, rms_in = 0, rms_out = 0;
    bool bad = false;
    for (size_t i = 0; i < w.samples.size(); i++) {
        if (!std::isfinite(o.samples[i])) bad = true;
        peak_in = std::max(peak_in, (double)fabs(w.samples[i])); peak_out = std::max(peak_out, (double)fabs(o.samples[i]));
        rms_in += w.samples[i] * (double)w.samples[i]; rms_out += o.samples[i] * (double)o.samples[i];
    }
    rms_in = sqrt(rms_in / w.samples.size()); rms_out = sqrt(rms_out / w.samples.size());
    printf("in : peak %.3f  rms %.4f (%.1f dBFS)\n", peak_in, rms_in, 20 * log10(rms_in + 1e-12));
    printf("out: peak %.3f  rms %.4f (%.1f dBFS)%s\n", peak_out, rms_out, 20 * log10(rms_out + 1e-12), bad ? "  !! NaN/Inf in output" : "");

    return write_wav_float(out, o) ? 0 : 1;
}
