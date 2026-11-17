/*
 * SysInfoLinux.cpp - Linux implementation of SysInfo (upstream Utils/SysInfo/SysInfo.cpp is Windows-only).
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "SysInfo.h"
#include "FxLinuxFeatures.h"

void SysInfo::enumAudioOutputs(StringArray& audioOutputNames) { audioOutputNames.clear(); }
bool SysInfo::canSupportHotkeys() { return FxLinux::Hotkeys::isSupported(); }
bool SysInfo::isRemoteSession() { return false; }
