/*
 * FxLinuxLog.cpp - see FxLinuxLog.h.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "FxLinuxLog.h"
#include <mutex>
#include <cstdio>

namespace
{
    std::mutex g_mu;
    const juce::int64 MAX_LOG = 1024 * 1024;

    juce::String readFirstLine(const juce::File &f, const juce::String &key)
    {
        juce::StringArray lines;
        f.readLines(lines);
        for (auto &l : lines)
            if (l.startsWith(key + "="))
                return l.fromFirstOccurrenceOf("=", false, false).unquoted();
        return {};
    }

    juce::String run(const juce::String &cmd)
    {
        juce::ChildProcess p;
        if (!p.start(cmd, juce::ChildProcess::wantStdOut)) return {};
        auto out = p.readAllProcessOutput();
        p.waitForProcessToFinish(2000);
        return out.trim();
    }

    class Forwarder : public juce::Logger
    {
        void logMessage(const juce::String &m) override { FxLinuxLog::write(m); }
    };
}

juce::File FxLinuxLog::logFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FxSound").getChildFile("fxsound.log");
}

void FxLinuxLog::write(const juce::String &message)
{
    auto line = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S ") + message.trimEnd();
    std::lock_guard<std::mutex> l(g_mu);
    if (std::getenv("FX_LOG_STDERR")) fprintf(stderr, "%s\n", line.toRawUTF8());
    auto f = logFile();
    f.getParentDirectory().createDirectory();
    if (f.getSize() > MAX_LOG) {
        auto old = f.getSiblingFile("fxsound.log.1");
        old.deleteFile();
        f.moveFileTo(old);
    }
    f.appendText(line + "\n", false, false, "\n");
}

juce::String FxLinuxLog::readAll()
{
    std::lock_guard<std::mutex> l(g_mu);
    auto f = logFile();
    auto old = f.getSiblingFile("fxsound.log.1");
    juce::String s;
    if (old.existsAsFile()) s << old.loadFileAsString();
    s << f.loadFileAsString();
    // keep the clipboard text reasonable: the last 300 KB
    if (s.length() > 300000) s = "...\n" + s.substring(s.length() - 300000);
    return s;
}

juce::String FxLinuxLog::systemReport(const juce::String &engine_diagnostics)
{
    juce::String r;
    r << "=== FxSound for Linux - diagnostic report ===\n";
    r << "Version: " << juce::JUCEApplication::getInstance()->getApplicationVersion() << "\n";
    auto os = juce::File("/etc/os-release");
    r << "Distro: " << (os.existsAsFile() ? readFirstLine(os, "PRETTY_NAME") : juce::String("unknown")) << "\n";
    r << "Kernel: " << run("uname -srm") << "\n";
    auto env = [](const char *k) { auto v = std::getenv(k); return juce::String(v ? v : ""); };
    r << "Desktop: " << env("XDG_CURRENT_DESKTOP") << " / session " << env("XDG_SESSION_TYPE") << "\n";
    r << "Packaging: " << (env("APPIMAGE").isNotEmpty() ? "AppImage" : "native") << "\n";
    r << "PipeWire: " << run("pipewire --version").replace("\n", " ") << "\n";
    r << "CPU: " << juce::SystemStats::getCpuModel() << " (" << juce::SystemStats::getNumCpus() << " threads)\n";
    r << "--- audio engine ---\n" << engine_diagnostics;
    r << "--- log ---\n";
    return r;
}

juce::Logger *FxLinuxLog::logger()
{
    static Forwarder f;
    return &f;
}
