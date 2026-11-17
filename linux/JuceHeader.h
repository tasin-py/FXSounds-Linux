/*
 * JuceHeader.h for the Linux CMake build (replaces the Projucer-generated one, same contents).
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#pragma once

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "BinaryData.h"

#if ! DONT_SET_USING_JUCE_NAMESPACE
 using namespace juce;
#endif

#if ! JUCE_DONT_DECLARE_PROJECTINFO
namespace ProjectInfo
{
    const char* const  projectName    = "FxSound";
    const char* const  companyName    = "FxSound LLC";
    const char* const  versionString  = FX_VERSION_STRING;
    const int          versionNumber  = FX_VERSION_NUMBER;
}
#endif
