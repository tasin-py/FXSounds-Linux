/*
 * FxLinuxCompat.h - the few Win32 names the FxSound GUI keeps using on Linux.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#pragma once
#include <cstdint>
#include "FxLockedDsp.h"
#include "FxLinuxLog.h"

typedef unsigned int UINT;
typedef uint32_t DWORD;
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
// RegisterHotKey modifier flags (stored in FxSound.settings, so keep the Windows values)
#ifndef MOD_ALT
#define MOD_ALT     0x0001
#define MOD_CONTROL 0x0002
#define MOD_SHIFT   0x0004
#endif
