/*
 * win32_misc.cpp - Linux implementations of the Win32 / MSVC CRT functions used by the
 * FxSound DSP and its helper libraries.
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "fx_linux_compat.h"
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <sys/time.h>

/* ------------------------------------------------------------------ UTF conversion */
static void utf8_append(std::string &out, uint32_t c)
{
    if (c < 0x80) out += (char)c;
    else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
    else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
}

static std::string to_utf8(const wchar_t *s, size_t n = (size_t)-1)
{
    std::string out;
    if (!s) return out;
    for (size_t i = 0; i < n && s[i]; i++) utf8_append(out, (uint32_t)s[i]);
    return out;
}

static std::wstring from_utf8(const char *s, size_t n = (size_t)-1)
{
    std::wstring out;
    if (!s) return out;
    size_t i = 0;
    while (i < n && s[i]) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { cp = c; extra = 0; } /* invalid lead byte: pass through as Latin-1 */
        i++;
        for (int k = 0; k < extra && i < n && s[i]; k++, i++) cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
        out += (wchar_t)cp;
    }
    return out;
}

extern "C" char *fx_path_to_utf8(const wchar_t *path)
{
    std::string p = to_utf8(path);
    for (auto &ch : p) if (ch == '\\') ch = '/';
    return strdup(p.c_str());
}

static std::string upath(const wchar_t *p)
{
    char *c = fx_path_to_utf8(p);
    std::string s(c); free(c);
    return s;
}

extern "C" int MultiByteToWideChar(UINT, DWORD, LPCSTR src, int srclen, LPWSTR dst, int dstlen)
{
    std::wstring w = from_utf8(src, srclen < 0 ? (size_t)-1 : (size_t)srclen);
    int needed = (int)w.size() + (srclen < 0 ? 1 : 0);
    if (dstlen == 0) return needed;
    if (needed > dstlen) return 0;
    memcpy(dst, w.data(), w.size() * sizeof(wchar_t));
    if (srclen < 0) dst[w.size()] = 0;
    return needed;
}

extern "C" int WideCharToMultiByte(UINT, DWORD, LPCWSTR src, int srclen, LPSTR dst, int dstlen, LPCSTR, BOOL *used_default)
{
    if (used_default) *used_default = FALSE;
    std::string s = to_utf8(src, srclen < 0 ? (size_t)-1 : (size_t)srclen);
    int needed = (int)s.size() + (srclen < 0 ? 1 : 0);
    if (dstlen == 0) return needed;
    if (needed > dstlen) return 0;
    memcpy(dst, s.data(), s.size());
    if (srclen < 0) dst[s.size()] = 0;
    return needed;
}

/* ------------------------------------------------------- MS-style wide printf/scanf */
/* In MSVC wide functions %s/%c take wide args and %S/%C narrow ones; glibc is the reverse
 * (%s narrow, %ls wide). Rewrite the format so upstream code behaves as on Windows. */
static std::wstring ms_fmt(const wchar_t *fmt, bool scanf_mode)
{
    std::wstring out;
    if (!fmt) return out;
    for (const wchar_t *p = fmt; *p; p++) {
        if (*p != L'%') { out += *p; continue; }
        out += *p++;
        if (*p == L'%') { out += *p; continue; }
        bool has_len = false;
        /* flags, width, precision (and scanf's '*' / scanset are passed through) */
        while (*p && wcschr(L"-+ #0123456789.*", *p)) out += *p++;
        if (*p == L'[') { /* scanset: MS wide -> wide; glibc needs %l[ */
            if (scanf_mode) out += L'l';
            while (*p && *p != L']') out += *p++;
            if (*p) out += *p;
            continue;
        }
        /* length modifiers */
        if (p[0] == L'I' && p[1] == L'6' && p[2] == L'4') { out += L"ll"; p += 3; has_len = true; }
        else if (p[0] == L'I' && p[1] == L'3' && p[2] == L'2') { p += 3; has_len = true; }
        else if (*p == L'I') { out += L"z"; p++; has_len = true; }
        while (*p && wcschr(L"hlLqjzt", *p)) { out += *p++; has_len = true; }
        if (!*p) break;
        wchar_t conv = *p;
        if ((conv == L's' || conv == L'c') && !has_len) { out += L'l'; out += conv; }
        else if (conv == L'S' && !has_len) out += L's';
        else if (conv == L'C' && !has_len) out += L'c';
        else out += conv;
    }
    return out;
}

extern "C" int fx_ms_vswprintf(wchar_t *buf, size_t count, const wchar_t *fmt, va_list ap)
{
    std::wstring f = ms_fmt(fmt, false);
    return ::vswprintf(buf, count, f.c_str(), ap);
}
extern "C" int fx_ms_vfwprintf(FILE *fp, const wchar_t *fmt, va_list ap)
{
    /* Write wide text as UTF-8 bytes, so the FILE stays byte-oriented like on Windows text mode */
    std::wstring f = ms_fmt(fmt, false);
    std::vector<wchar_t> tmp(65536);
    va_list ap2; va_copy(ap2, ap);
    int n = ::vswprintf(tmp.data(), tmp.size(), f.c_str(), ap2);
    va_end(ap2);
    if (n < 0) return n;
    std::string s = to_utf8(tmp.data());
    fwrite(s.data(), 1, s.size(), fp);
    return n;
}
extern "C" int fx_ms_vswscanf(const wchar_t *s, const wchar_t *fmt, va_list ap)
{
    std::wstring f = ms_fmt(fmt, true);
    return ::vswscanf(s, f.c_str(), ap);
}
extern "C" int fx_ms_vfwscanf(FILE *fp, const wchar_t *fmt, va_list ap)
{
    /* Files are read as UTF-8 bytes. Read one line and scan it - upstream only uses
     * fwscanf for simple one-value-per-line reads. */
    char line[4096];
    if (!fgets(line, sizeof line, fp)) return EOF;
    std::wstring w = from_utf8(line);
    return fx_ms_vswscanf(w.c_str(), fmt, ap);
}
extern "C" int fx_ms_swprintf_s(wchar_t *buf, size_t count, const wchar_t *fmt, ...)
{ va_list ap; va_start(ap, fmt); int r = fx_ms_vswprintf(buf, count, fmt, ap); va_end(ap); return r; }
extern "C" int fx_ms_fwprintf(FILE *f, const wchar_t *fmt, ...)
{ va_list ap; va_start(ap, fmt); int r = fx_ms_vfwprintf(f, fmt, ap); va_end(ap); return r; }
extern "C" int fx_ms_fwscanf(FILE *f, const wchar_t *fmt, ...)
{ va_list ap; va_start(ap, fmt); int r = fx_ms_vfwscanf(f, fmt, ap); va_end(ap); return r; }
extern "C" int fx_ms_swscanf(const wchar_t *s, const wchar_t *fmt, ...)
{ va_list ap; va_start(ap, fmt); int r = fx_ms_vswscanf(s, fmt, ap); va_end(ap); return r; }
extern "C" int fx_ms_wprintf(const wchar_t *fmt, ...)
{ va_list ap; va_start(ap, fmt); int r = fx_ms_vfwprintf(stdout, fmt, ap); va_end(ap); return r; }

/* ------------------------------------------------------------------- file system */
extern "C" FILE *_wfopen(const wchar_t *path, const wchar_t *mode)
{
    std::string m = to_utf8(mode);
    /* drop MS-only mode flags: ccs=..., t, N, etc. */
    std::string clean;
    for (char c : m) { if (c == ',') break; if (strchr("rwab+", c)) clean += c; }
    return fopen(upath(path).c_str(), clean.c_str());
}
extern "C" int _wremove(const wchar_t *p) { return remove(upath(p).c_str()); }
extern "C" int _wunlink(const wchar_t *p) { return unlink(upath(p).c_str()); }
extern "C" int _wrename(const wchar_t *a, const wchar_t *b) { return rename(upath(a).c_str(), upath(b).c_str()); }
extern "C" int _wmkdir(const wchar_t *p) { return mkdir(upath(p).c_str(), 0755); }
extern "C" int _waccess(const wchar_t *p, int mode) { return access(upath(p).c_str(), mode); }
extern "C" int _wstat(const wchar_t *p, struct stat *st) { return stat(upath(p).c_str(), st); }
extern "C" int _wstati64(const wchar_t *p, struct stat *st) { return stat(upath(p).c_str(), st); }
extern "C" BOOL PathFileExistsW(LPCWSTR p) { return access(upath(p).c_str(), F_OK) == 0; }
extern "C" BOOL CreateDirectoryW(LPCWSTR p, LPSECURITY_ATTRIBUTES) { return mkdir(upath(p).c_str(), 0755) == 0; }

extern "C" wchar_t *_wcserror(int e)
{
    static thread_local wchar_t buf[256];
    std::wstring w = from_utf8(strerror(e));
    wcsncpy(buf, w.c_str(), 255); buf[255] = 0;
    return buf;
}

extern "C" void _wsplitpath(const wchar_t *path, wchar_t *drive, wchar_t *dir, wchar_t *fname, wchar_t *ext)
{
    std::wstring p = path ? path : L"";
    std::wstring d;
    if (p.size() >= 2 && p[1] == L':') { d = p.substr(0, 2); p = p.substr(2); }
    size_t slash = p.find_last_of(L"/\\");
    std::wstring dirpart = (slash == std::wstring::npos) ? L"" : p.substr(0, slash + 1);
    std::wstring file = (slash == std::wstring::npos) ? p : p.substr(slash + 1);
    size_t dot = file.find_last_of(L'.');
    std::wstring name = (dot == std::wstring::npos) ? file : file.substr(0, dot);
    std::wstring e = (dot == std::wstring::npos) ? L"" : file.substr(dot);
    if (drive) wcscpy(drive, d.c_str());
    if (dir) wcscpy(dir, dirpart.c_str());
    if (fname) wcscpy(fname, name.c_str());
    if (ext) wcscpy(ext, e.c_str());
}

extern "C" LONG CompareFileTime(const FILETIME *a, const FILETIME *b)
{
    uint64_t x = ((uint64_t)a->dwHighDateTime << 32) | a->dwLowDateTime;
    uint64_t y = ((uint64_t)b->dwHighDateTime << 32) | b->dwLowDateTime;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* ------------------------------------------------------------------- strings */
extern "C" long _wtol(const wchar_t *s) { return wcstol(s, nullptr, 10); }
extern "C" int _wtoi(const wchar_t *s) { return (int)wcstol(s, nullptr, 10); }
extern "C" double _wtof(const wchar_t *s) { return wcstod(s, nullptr); }

template <class C> static C *itoa_impl(long v, C *buf, int radix)
{
    char tmp[72]; int i = 0; bool neg = (v < 0 && radix == 10);
    unsigned long u = neg ? (unsigned long)(-v) : (unsigned long)v;
    do { int d = (int)(u % radix); tmp[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); u /= radix; } while (u);
    int j = 0; if (neg) buf[j++] = '-';
    while (i) buf[j++] = (C)tmp[--i];
    buf[j] = 0; return buf;
}
extern "C" char *_itoa(int v, char *b, int r) { return itoa_impl<char>(v, b, r); }
extern "C" char *_ltoa(long v, char *b, int r) { return itoa_impl<char>(v, b, r); }
extern "C" wchar_t *_itow(int v, wchar_t *b, int r) { return itoa_impl<wchar_t>(v, b, r); }
extern "C" wchar_t *_ltow(long v, wchar_t *b, int r) { return itoa_impl<wchar_t>(v, b, r); }
extern "C" char *_strupr(char *s) { for (char *p = s; *p; p++) *p = (char)toupper((unsigned char)*p); return s; }
extern "C" char *_strlwr(char *s) { for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p); return s; }
extern "C" wchar_t *_wcsupr(wchar_t *s) { for (wchar_t *p = s; *p; p++) *p = towupper(*p); return s; }
extern "C" wchar_t *_wcslwr(wchar_t *s) { for (wchar_t *p = s; *p; p++) *p = towlower(*p); return s; }
extern "C" int _wcsicmp(const wchar_t *a, const wchar_t *b) { return wcscasecmp(a, b); }
extern "C" int _wcsnicmp(const wchar_t *a, const wchar_t *b, size_t n) { return wcsncasecmp(a, b, n); }
extern "C" wchar_t *_wcsdup(const wchar_t *s) { return wcsdup(s); }
extern "C" char *_strdup(const char *s) { return strdup(s); }

/* ------------------------------------------------------------------- misc */
static bool quiet() { const char *q = getenv("FX_QUIET"); return q && *q == '1'; }

extern "C" int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT)
{
    if (!quiet()) fprintf(stderr, "[fxsound] %s: %s\n", caption ? caption : "", text ? text : "");
    return IDOK;
}
extern "C" int MessageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT)
{
    if (!quiet()) fprintf(stderr, "[fxsound] %s: %s\n", to_utf8(caption).c_str(), to_utf8(text).c_str());
    return IDOK;
}
extern "C" DWORD FormatMessageW(DWORD, LPCVOID, DWORD id, DWORD, LPWSTR buf, DWORD size, va_list *)
{
    std::wstring w = from_utf8(strerror((int)id));
    if (!buf || size == 0) return 0;
    wcsncpy(buf, w.c_str(), size - 1); buf[size - 1] = 0;
    return (DWORD)wcslen(buf);
}

static uint64_t now_ms()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
extern "C" DWORD GetTickCount(void) { return (DWORD)now_ms(); }
extern "C" ULONGLONG GetTickCount64(void) { return now_ms(); }
extern "C" DWORD timeGetTime(void) { return (DWORD)now_ms(); }
extern "C" void Sleep(DWORD ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
extern "C" DWORD GetLastError(void) { return (DWORD)errno; }
extern "C" void OutputDebugStringA(LPCSTR s) { if (getenv("FX_DEBUG")) fprintf(stderr, "%s", s); }
extern "C" void OutputDebugStringW(LPCWSTR s) { if (getenv("FX_DEBUG")) fprintf(stderr, "%s", to_utf8(s).c_str()); }
extern "C" BOOL IsDebuggerPresent(void) { return FALSE; }
extern "C" void DebugBreak(void) {}

static void fill_systime(LPSYSTEMTIME st, bool local)
{
    struct timeval tv; gettimeofday(&tv, nullptr);
    struct tm t; time_t s = tv.tv_sec;
    if (local) localtime_r(&s, &t); else gmtime_r(&s, &t);
    st->wYear = (WORD)(t.tm_year + 1900); st->wMonth = (WORD)(t.tm_mon + 1); st->wDayOfWeek = (WORD)t.tm_wday;
    st->wDay = (WORD)t.tm_mday; st->wHour = (WORD)t.tm_hour; st->wMinute = (WORD)t.tm_min;
    st->wSecond = (WORD)t.tm_sec; st->wMilliseconds = (WORD)(tv.tv_usec / 1000);
}
extern "C" void GetLocalTime(LPSYSTEMTIME st) { fill_systime(st, true); }
extern "C" void GetSystemTime(LPSYSTEMTIME st) { fill_systime(st, false); }
extern "C" BOOL QueryPerformanceCounter(LARGE_INTEGER *c)
{
    c->QuadPart = (LONGLONG)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return TRUE;
}
extern "C" BOOL QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000000LL; return TRUE; }

extern "C" DWORD GetModuleFileNameW(HMODULE, LPWSTR buf, DWORD size)
{
    char p[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", p, sizeof p - 1);
    if (n < 0 || size == 0) return 0;
    p[n] = 0;
    std::wstring w = from_utf8(p);
    wcsncpy(buf, w.c_str(), size - 1); buf[size - 1] = 0;
    return (DWORD)wcslen(buf);
}
extern "C" DWORD GetModuleFileNameA(HMODULE, LPSTR buf, DWORD size)
{
    ssize_t n = readlink("/proc/self/exe", buf, size ? size - 1 : 0);
    if (n < 0) return 0;
    buf[n] = 0; return (DWORD)n;
}
extern "C" BOOL CloseHandle(HANDLE) { return TRUE; }
extern "C" BOOL FreeLibrary(HMODULE) { return TRUE; }

extern "C" HRESULT CoCreateGuid(GUID *g)
{
    std::random_device rd; std::mt19937_64 gen(((uint64_t)rd() << 32) ^ rd());
    uint64_t a = gen(), b = gen();
    memcpy(g, &a, 8); memcpy((char *)g + 8, &b, 8);
    g->Data3 = (uint16_t)((g->Data3 & 0x0FFF) | 0x4000);
    g->Data4[0] = (uint8_t)((g->Data4[0] & 0x3F) | 0x80);
    return S_OK;
}
int StringFromGUID2(const GUID &gr, LPWSTR buf, int size)
{
    const GUID *g = &gr;
    if (size < 39) return 0;
    ::swprintf(buf, size, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
               g->Data1, g->Data2, g->Data3, g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
               g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
    return 39;
}

/* ------------------------------------------------- FILE/FileDate.cpp replacement */
class CSlout;
int fileGetModifiedDate_Wide(wchar_t *path, FILETIME *ft, CSlout *)
{
    struct stat st;
    if (_wstat(path, &st) != 0) return 1; /* NOT_OKAY */
    /* FILETIME = 100 ns intervals since 1601-01-01 */
    uint64_t t = ((uint64_t)st.st_mtim.tv_sec + 11644473600ULL) * 10000000ULL + st.st_mtim.tv_nsec / 100;
    ft->dwLowDateTime = (DWORD)t; ft->dwHighDateTime = (DWORD)(t >> 32);
    return 0;
}

/* --------------------------------------- operatingSystem/*.cpp replacement */
int operatingSystemIsWow64(int *ip_is_wow64) { *ip_is_wow64 = 0; return 0; }

/* ------------------------------------------- Windows text-mode style line reads */
#undef fgets
#undef fgetws
#undef fputws
extern "C" char *fx_fgets(char *buf, int n, FILE *f)
{
    if (!fgets(buf, n, f)) return nullptr;
    size_t len = strlen(buf);
    if (len >= 2 && buf[len - 2] == '\r' && buf[len - 1] == '\n') { buf[len - 2] = '\n'; buf[len - 1] = 0; }
    else if (len >= 1 && buf[len - 1] == '\r') buf[len - 1] = 0; /* last line without LF */
    return buf;
}
extern "C" wchar_t *fx_fgetws(wchar_t *buf, int n, FILE *f)
{
    if (n <= 0) return nullptr;
    std::vector<char> tmp((size_t)n * 4 + 4);
    if (!fx_fgets(tmp.data(), (int)tmp.size(), f)) return nullptr;
    std::wstring w = from_utf8(tmp.data());
    if (w.size() > (size_t)n - 1) w.resize((size_t)n - 1);
    wcscpy(buf, w.c_str());
    return buf;
}
extern "C" int fx_fputws(const wchar_t *s, FILE *f)
{
    std::string u = to_utf8(s);
    return fputs(u.c_str(), f) < 0 ? -1 : 1;
}
