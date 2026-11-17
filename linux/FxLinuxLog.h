/*
 * FxLinuxLog.h - application log for the Linux port: written to ~/.config/FxSound/fxsound.log
 * (rotated at 1 MB) and kept in memory, so "Copy log" can put it on the clipboard for bug reports.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#pragma once
#include <JuceHeader.h>

namespace FxLinuxLog
{
    juce::File logFile();
    // Thread-safe. Adds a timestamp.
    void write(const juce::String &message);
    // Whole current log (previous rotated log first), for the clipboard.
    juce::String readAll();
    // Header with app version, distro, desktop, PipeWire, plus the given engine diagnostics.
    juce::String systemReport(const juce::String &engine_diagnostics);
    // juce::Logger that forwards to write() (FxController's logMessage()).
    juce::Logger *logger();
}
