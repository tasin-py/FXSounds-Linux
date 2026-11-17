/*
 * fxsound-engine - headless FxSound for Linux (test / CLI front end of FxEngine).
 *
 *   fxsound-engine [--preset FILE.fac] [--off] [--no-default] [--output NODE_NAME]
 *
 * Reads commands on stdin (one per line):
 *   preset FILE.fac | power on|off | effect 0..4 VALUE(0..10) | eq BAND DB
 *   output NODE_NAME|auto | devices | status | quit
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "fx_engine.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <unistd.h>

static std::atomic<bool> g_quit{false};
static void on_signal(int) { g_quit = true; }

int main(int argc, char **argv)
{
    std::setlocale(LC_ALL, "C.UTF-8");
    std::string preset, output;
    bool off = false, no_default = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--preset" && i + 1 < argc) preset = argv[++i];
        else if (a == "--output" && i + 1 < argc) output = argv[++i];
        else if (a == "--off") off = true;
        else if (a == "--no-default") no_default = true;
        else { fprintf(stderr, "usage: %s [--preset FILE.fac] [--off] [--no-default] [--output NODE]\n", argv[0]); return 2; }
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal);

    FxEngine eng;
    if (!preset.empty() && eng.loadPreset(preset)) printf("preset: %s\n", eng.presetName(preset).c_str());
    eng.powerOn(!off);
    std::string err;
    if (!eng.start(err)) { fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    if (no_default) eng.claimDefault(false);
    if (!output.empty()) eng.setOutputDevice(output);

    std::string line;
    while (!g_quit) {
        pollfd p{0, POLLIN, 0};
        int r = poll(&p, 1, 200);
        if (eng.connectionLost() || !eng.isRunning()) {
            // PipeWire restarted: reconnect (retry every second until it is back)
            static auto next = std::chrono::steady_clock::now();
            if (std::chrono::steady_clock::now() >= next) {
                eng.stop();
                std::string e;
                if (eng.start(e)) { if (no_default) eng.claimDefault(false); if (!output.empty()) eng.setOutputDevice(output); }
                else FxEngine::log("reconnect failed: %s", e.c_str());
                next = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
        }
        if (r <= 0) continue;
        if (!std::getline(std::cin, line)) {
            // stdin closed: keep running until a signal arrives
            while (!g_quit) pause();
            break;
        }
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd == "quit" || cmd == "exit") break;
        else if (cmd == "preset") { std::string f; std::getline(ss >> std::ws, f); printf(eng.loadPreset(f) ? "ok %s\n" : "failed %s\n", eng.presetName(f).c_str()); }
        else if (cmd == "power") { std::string v; ss >> v; eng.powerOn(v == "on"); printf("power %s\n", eng.isPowerOn() ? "on" : "off"); }
        else if (cmd == "effect") { int e; float v; ss >> e >> v; eng.setEffectValue((FxEngine::Effect)e, v); printf("ok\n"); }
        else if (cmd == "eq") { int b; float v; ss >> b >> v; eng.setEqBandBoostCut(b, v); printf("ok\n"); }
        else if (cmd == "output") { std::string n; ss >> n; eng.setOutputDevice(n == "auto" ? "" : n); printf("output %s\n", eng.outputDevice().c_str()); }
        else if (cmd == "devices") {
            for (auto &d : eng.outputDevices())
                printf("%s %-50s %s\n", d.name == eng.outputDevice() ? "*" : " ", d.name.c_str(), d.description.c_str());
        } else if (cmd == "status") {
            printf("power %s, rate %u, frames %llu, dsp load %.2f%%, output %s\n", eng.isPowerOn() ? "on" : "off",
                   eng.sampleRate(), (unsigned long long)eng.framesProcessed(), eng.cpuLoad() * 100, eng.outputDevice().c_str());
            printf("%s\n", eng.debugStats().c_str());
        } else if (!cmd.empty()) printf("unknown command\n");
        fflush(stdout);
    }
    eng.stop();
    return 0;
}
