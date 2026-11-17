/*
 * fx_linux_compat.h - minimal Win32 surface needed to build the FxSound DSP on Linux.
 * Force-included into every translation unit (see Makefile).
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#ifndef FX_LINUX_COMPAT_H
#define FX_LINUX_COMPAT_H

#ifndef FX_LINUX
#define FX_LINUX 1
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <stdarg.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef __cplusplus
/* libstdc++'s <cstdio>/<cwchar>/... #undef the C function names. Include them now, before
 * our macros below, so later includes are no-ops and don't cancel the replacements. */
#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <ctime>
#include <cstdarg>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#endif

/* ---- 32-bit "long" used by the FxSound sources (see tools/convert_long.py) ---- */
typedef int32_t  pt_long;
typedef uint32_t pt_ulong;

/* ---- basic Win32 types ---- */
typedef int                BOOL;
typedef unsigned char      BYTE;
typedef unsigned short     WORD;
typedef uint32_t           DWORD;
typedef uint32_t           UINT32;
typedef int32_t            INT32;
typedef int64_t            LONGLONG;
typedef uint64_t           ULONGLONG;
typedef uint64_t           DWORD64;
typedef unsigned int       UINT;
typedef long               LONG;
typedef unsigned long      ULONG;
typedef short              SHORT;
typedef unsigned short     USHORT;
typedef char               CHAR;
typedef unsigned char      UCHAR;
typedef wchar_t            WCHAR;
typedef wchar_t            TCHAR;
typedef float              FLOAT;
typedef void               VOID;
typedef void              *PVOID;
typedef void              *LPVOID;
typedef const void        *LPCVOID;
typedef char              *LPSTR;
typedef const char        *LPCSTR;
typedef wchar_t           *LPWSTR;
typedef const wchar_t     *LPCWSTR;
typedef wchar_t           *LPTSTR;
typedef const wchar_t     *LPCTSTR;
typedef DWORD             *LPDWORD;
typedef DWORD             *PDWORD;
typedef BYTE              *LPBYTE;
typedef long               LRESULT;
typedef long               HRESULT;
typedef intptr_t           LPARAM;
typedef uintptr_t          WPARAM;
typedef uintptr_t          UINT_PTR;
typedef intptr_t           INT_PTR;
typedef intptr_t           LONG_PTR;
typedef uintptr_t          ULONG_PTR;
typedef uintptr_t          DWORD_PTR;
typedef size_t             SIZE_T;
typedef void              *HANDLE;
typedef void              *HINSTANCE;
typedef void              *HMODULE;
typedef void              *HKEY;
typedef HKEY              *PHKEY;
typedef void              *HWND;
typedef void              *HDC;
typedef void              *HMENU;
typedef void              *HICON;
typedef void              *HBITMAP;
typedef void              *HFONT;
typedef DWORD              REGSAM;
typedef DWORD              COLORREF;
typedef LONG               LSTATUS;

typedef struct _FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME, *LPFILETIME, *PFILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;
typedef struct _RECT { LONG left, top, right, bottom; } RECT, *LPRECT;
typedef struct _POINT { LONG x, y; } POINT, *LPPOINT;
typedef union _LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; } u; LONGLONG QuadPart; } LARGE_INTEGER;
typedef struct _SECURITY_ATTRIBUTES { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; } SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

#define __int64 long long
typedef char              *HPSTR;

/* ---- multimedia (mmsystem.h / mmreg.h) types; only used as struct members here ---- */
typedef void *HMMIO;
typedef void *HWAVEOUT;
typedef void *HWAVEIN;
typedef DWORD FOURCC;
typedef struct tWAVEFORMATEX {
    WORD nFormatTag; WORD nChannels; DWORD nSamplesPerSec; DWORD nAvgBytesPerSec;
    WORD nBlockAlign; WORD wBitsPerSample; WORD cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;
typedef struct { WAVEFORMATEX Format; union { WORD wValidBitsPerSample; WORD wSamplesPerBlock; WORD wReserved; } Samples;
    DWORD dwChannelMask; unsigned char SubFormat[16]; } WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
typedef struct _MMCKINFO { FOURCC ckid; DWORD cksize; FOURCC fccType; DWORD dwDataOffset; DWORD dwFlags; } MMCKINFO, *LPMMCKINFO;
typedef struct wavehdr_tag {
    LPSTR lpData; DWORD dwBufferLength; DWORD dwBytesRecorded; DWORD_PTR dwUser; DWORD dwFlags;
    DWORD dwLoops; struct wavehdr_tag *lpNext; DWORD_PTR reserved;
} WAVEHDR, *PWAVEHDR, *LPWAVEHDR;
#define WAVE_FORMAT_PCM        1
#define WAVE_FORMAT_IEEE_FLOAT 3
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define CALLBACK
#define CONST const
#define DELETE 0x00010000L
#define ERROR_BADKEY 1010L
#define lstrlenW(s) ((int)wcslen(s))
#define lstrlen lstrlenW
#define WINAPI
#define APIENTRY
#define __stdcall
#define __cdecl
#define _cdecl
#define __forceinline inline
#define __declspec(x)
#define _In_
#define _In_opt_
#define _Out_
#define _Inout_
#define MAX_PATH 260
#define _MAX_PATH 260
#define _MAX_FNAME 256
#define _MAX_EXT 256
#define _MAX_DIR 256
#define _MAX_DRIVE 3
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define S_OK 0
#define S_FALSE 1
#define E_FAIL ((HRESULT)0x80004005L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_MORE_DATA 234L
#define ERROR_NO_MORE_ITEMS 259L
#define MB_OK 0
#define MB_ICONERROR 0x10
#define MB_ICONWARNING 0x30
#define MB_ICONINFORMATION 0x40
#define MB_YESNO 4
#define MB_TOPMOST 0x40000
#define IDYES 6
#define IDNO 7
#define IDOK 1
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define _T(x) L##x
#define TEXT(x) L##x
#define __T(x) L##x

/* ---- registry (implemented in compat/registry.cpp as an in-memory store) ---- */
#define HKEY_CLASSES_ROOT   ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER   ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE  ((HKEY)(uintptr_t)0x80000002)
#define HKEY_USERS          ((HKEY)(uintptr_t)0x80000003)
#define KEY_READ            0x20019
#define KEY_WRITE           0x20006
#define KEY_ALL_ACCESS      0xF003F
#define KEY_QUERY_VALUE     0x0001
#define KEY_SET_VALUE       0x0002
#define KEY_ENUMERATE_SUB_KEYS 0x0008
#define KEY_WOW64_64KEY     0x0100
#define KEY_WOW64_32KEY     0x0200
#define REG_NONE            0
#define REG_SZ              1
#define REG_EXPAND_SZ       2
#define REG_BINARY          3
#define REG_DWORD           4
#define REG_QWORD           11
#define REG_OPTION_NON_VOLATILE 0
#define REG_CREATED_NEW_KEY 1

#ifdef __cplusplus
extern "C" {
#endif
LONG RegOpenKeyExW(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
LONG RegCreateKeyExW(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
LONG RegCreateKeyExA(HKEY, LPCSTR, DWORD, LPSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
LONG RegQueryValueExW(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
LONG RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE *, DWORD);
LONG RegSetValueExA(HKEY, LPCSTR, DWORD, DWORD, const BYTE *, DWORD);
LONG RegDeleteValueW(HKEY, LPCWSTR);
LONG RegDeleteValueA(HKEY, LPCSTR);
LONG RegDeleteKeyW(HKEY, LPCWSTR);
LONG RegDeleteKeyA(HKEY, LPCSTR);
LONG RegEnumKeyExW(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME);
LONG RegEnumKeyExA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, PFILETIME);
LONG RegEnumValueW(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
LONG RegEnumValueA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
LONG RegFlushKey(HKEY);
LONG RegCloseKey(HKEY);

/* ---- misc Win32 functions (compat/win32_misc.cpp) ---- */
int   MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
int   MessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT);
DWORD GetTickCount(void);
ULONGLONG GetTickCount64(void);
DWORD timeGetTime(void);
void  Sleep(DWORD ms);
DWORD GetLastError(void);
void  OutputDebugStringA(LPCSTR);
void  OutputDebugStringW(LPCWSTR);
BOOL  IsDebuggerPresent(void);
void  DebugBreak(void);
void  GetLocalTime(LPSYSTEMTIME);
void  GetSystemTime(LPSYSTEMTIME);
BOOL  QueryPerformanceCounter(LARGE_INTEGER *);
BOOL  QueryPerformanceFrequency(LARGE_INTEGER *);
DWORD GetModuleFileNameW(HMODULE, LPWSTR, DWORD);
DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD);
BOOL  CloseHandle(HANDLE);
BOOL  FreeLibrary(HMODULE);

/* MSVC CRT names */
FILE *_wfopen(const wchar_t *path, const wchar_t *mode);
int   _wremove(const wchar_t *path);
int   _wrename(const wchar_t *from, const wchar_t *to);
int   _wmkdir(const wchar_t *path);
int   _waccess(const wchar_t *path, int mode);
long  _wtol(const wchar_t *s);
int   _wtoi(const wchar_t *s);
double _wtof(const wchar_t *s);
char *_itoa(int v, char *buf, int radix);
char *_ltoa(long v, char *buf, int radix);
wchar_t *_itow(int v, wchar_t *buf, int radix);
wchar_t *_ltow(long v, wchar_t *buf, int radix);
char *_strupr(char *s);
char *_strlwr(char *s);
wchar_t *_wcsupr(wchar_t *s);
wchar_t *_wcslwr(wchar_t *s);
int   _wcsicmp(const wchar_t *a, const wchar_t *b);
int   _wcsnicmp(const wchar_t *a, const wchar_t *b, size_t n);
wchar_t *_wcsdup(const wchar_t *s);
char *_strdup(const char *s);
/* MS-style printf family: %s/%c in a wide format means wide, %S means narrow */
int   fx_ms_vswprintf(wchar_t *buf, size_t count, const wchar_t *fmt, va_list ap);
int   fx_ms_vfwprintf(FILE *f, const wchar_t *fmt, va_list ap);
int   fx_ms_vfwscanf(FILE *f, const wchar_t *fmt, va_list ap);
int   fx_ms_vswscanf(const wchar_t *s, const wchar_t *fmt, va_list ap);
int   fx_ms_swprintf_s(wchar_t *buf, size_t count, const wchar_t *fmt, ...);
int   fx_ms_fwprintf(FILE *f, const wchar_t *fmt, ...);
int   fx_ms_fwscanf(FILE *f, const wchar_t *fmt, ...);
int   fx_ms_swscanf(const wchar_t *s, const wchar_t *fmt, ...);
int   fx_ms_wprintf(const wchar_t *fmt, ...);

/* Text-file reads behave like Windows text mode: CRLF -> LF, and wide reads decode UTF-8
 * from a byte-oriented stream (glibc forbids mixing fgets/fgetws on one FILE). */
char    *fx_fgets(char *buf, int n, FILE *f);
wchar_t *fx_fgetws(wchar_t *buf, int n, FILE *f);
int      fx_fputws(const wchar_t *s, FILE *f);

/* file system (paths are converted wchar_t -> UTF-8, '\\' -> '/') */
typedef int errno_t;
#define _stat64i32 stat
#define _stati64   stat
int   _wstat(const wchar_t *path, struct stat *st);
int   _wstati64(const wchar_t *path, struct stat *st);
int   _wunlink(const wchar_t *path);
void  _wsplitpath(const wchar_t *path, wchar_t *drive, wchar_t *dir, wchar_t *fname, wchar_t *ext);
wchar_t *_wcserror(int errnum);
BOOL  PathFileExistsW(LPCWSTR path);
BOOL  CreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES);
LONG  CompareFileTime(const FILETIME *a, const FILETIME *b);
/* fx_path_to_utf8: convert a Windows-style wide path to a Linux UTF-8 path. Caller frees. */
char *fx_path_to_utf8(const wchar_t *path);

/* character set conversion (wchar_t on Linux is UTF-32; narrow strings are UTF-8) */
#define CP_ACP  0
#define CP_UTF8 65001
#define MB_PRECOMPOSED 1
#define WC_NO_BEST_FIT_CHARS 0x400
int   MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR src, int srclen, LPWSTR dst, int dstlen);
int   WideCharToMultiByte(UINT cp, DWORD flags, LPCWSTR src, int srclen, LPSTR dst, int dstlen, LPCSTR defchar, BOOL *used_default);

/* misc */
#define FORMAT_MESSAGE_FROM_SYSTEM 0x1000
DWORD FormatMessageW(DWORD flags, LPCVOID src, DWORD id, DWORD lang, LPWSTR buf, DWORD size, va_list *args);
#define FormatMessage FormatMessageW
typedef struct _GUID { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; } GUID;
HRESULT CoCreateGuid(GUID *g);
#ifdef __cplusplus
}
int StringFromGUID2(const GUID &g, LPWSTR buf, int size); /* REFGUID */
#endif
#define MessageBox MessageBoxW
#define OutputDebugString OutputDebugStringW
#define GetModuleFileName GetModuleFileNameW
#define RegOpenKeyEx RegOpenKeyExW
#define RegCreateKeyEx RegCreateKeyExW
#define RegQueryValueEx RegQueryValueExW
#define RegSetValueEx RegSetValueExW
#define RegDeleteValue RegDeleteValueW
#define RegDeleteKey RegDeleteKeyW
#define RegEnumKeyEx RegEnumKeyExW
#define RegEnumValue RegEnumValueW
#define fgets  fx_fgets
#define fgetws fx_fgetws
#define fputws fx_fputws
#define wcstok_s(s, delim, ctx) wcstok((s), (delim), (ctx))
#define wsprintf  swprintf
#define wsprintfW swprintf

#define _stricmp  strcasecmp
#define _strnicmp strncasecmp
#define stricmp   strcasecmp
#define strnicmp  strncasecmp
#define _wcsicmp  _wcsicmp
#define wcsicmp   _wcsicmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define sprintf_s snprintf
#define _access   access
#define _unlink   unlink
#define _getcwd   getcwd
#define _fileno   fileno
#define _isnan    isnan
#define _finite   isfinite
#define _hypot    hypot
#define strcpy_s(d, n, s)       (strncpy((d), (s), (n)), (d)[(n) - 1] = 0, 0)
#define strcat_s(d, n, s)       (strncat((d), (s), (n) - strlen(d) - 1), 0)
#define wcscpy_s(d, n, s)       (wcsncpy((d), (s), (n)), (d)[(n) - 1] = 0, 0)
#define wcscat_s(d, n, s)       (wcsncat((d), (s), (n) - wcslen(d) - 1), 0)
#define wcsncpy_s(d, n, s, c)   (wcsncpy((d), (s), ((size_t)(c) < (size_t)(n) ? (size_t)(c) : (size_t)(n) - 1)), (d)[((size_t)(c) < (size_t)(n) ? (size_t)(c) : (size_t)(n) - 1)] = 0, 0)
#define strncpy_s(d, n, s, c)   (strncpy((d), (s), ((size_t)(c) < (size_t)(n) ? (size_t)(c) : (size_t)(n) - 1)), (d)[((size_t)(c) < (size_t)(n) ? (size_t)(c) : (size_t)(n) - 1)] = 0, 0)
#define _wfopen_s(pf, p, m)     ((*(pf) = _wfopen((p), (m))) ? 0 : errno)
#define fopen_s(pf, p, m)       ((*(pf) = fopen((p), (m))) ? 0 : errno)
#define swprintf_s              fx_ms_swprintf_s
#define fwprintf                fx_ms_fwprintf
#define fwscanf                 fx_ms_fwscanf
#define swscanf                 fx_ms_swscanf
#define wprintf                 fx_ms_wprintf
#define vswprintf_s(b, n, f, a) fx_ms_vswprintf((b), (n), (f), (a))
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#ifndef __cplusplus
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#define ZeroMemory(p, n) memset((p), 0, (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define FillMemory(d, n, v) memset((d), (v), (n))

#ifdef __cplusplus
/* MSVC with _CRT_NON_CONFORMING_SWPRINTFS: swprintf(buf, fmt, ...) has no size argument.
 * Provide that overload; the standard (buf, size, fmt, ...) overload still exists. */
#include <cwchar>
inline int swprintf(wchar_t *buf, const wchar_t *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = fx_ms_vswprintf(buf, 32768, fmt, ap);
    va_end(ap); return r;
}
inline int vswprintf(wchar_t *buf, const wchar_t *fmt, va_list ap)
{
    return fx_ms_vswprintf(buf, 32768, fmt, ap);
}
/* C++: Windows min/max macros would break the standard library; use templates that accept mixed types */
template <class A, class B> inline auto min(A a, B b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template <class A, class B> inline auto max(A a, B b) -> decltype(a > b ? a : b) { return a > b ? a : b; }
#endif

#endif /* FX_LINUX_COMPAT_H */
