/*
 * FxGuiPrelude.h - force-included into the GUI build on Linux: the MSVC/Windows conveniences the
 * upstream GUI code relies on (WCHAR, _countof, min/max from <windows.h>).
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#pragma once
#ifdef __cplusplus
#include <algorithm>
#include <cstddef>
typedef wchar_t WCHAR;
#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif
#include <cwchar>
// only reached in dead Windows code paths; keeps them compiling
#define swprintf_s(buf, fmt, ...) swprintf(buf, sizeof(buf) / sizeof(buf[0]), fmt, __VA_ARGS__)
using std::min;
using std::max;
#endif
