/*
 * registry.cpp - in-memory replacement for the Windows registry.
 *
 * The FxSound DSP stores session state (current EQ, effect values, ...) in the registry.
 * On Linux we keep it in a process-local map. (A later step can persist it to
 * ~/.config/fxsound if the GUI needs that.)
 *
 * Part of the FxSound Linux port. Licensed AGPL-3.0-or-later like upstream FxSound.
 */
#include "fx_linux_compat.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>

namespace {

struct Value { DWORD type; std::vector<BYTE> data; };
struct Key   { std::map<std::wstring, Value> values; };

std::mutex g_mu;
std::map<std::wstring, Key> g_keys;          // key = lowercase full path, e.g. "hkcu\software\dfx"

struct Handle { std::wstring path; };

std::wstring lower(std::wstring s) { for (auto &c : s) c = towlower(c); return s; }

std::wstring root_name(HKEY k)
{
    switch ((uintptr_t)k) {
    case 0x80000000: return L"hkcr";
    case 0x80000001: return L"hkcu";
    case 0x80000002: return L"hklm";
    case 0x80000003: return L"hku";
    default: return L"";
    }
}

bool resolve(HKEY parent, const wchar_t *sub, std::wstring &out)
{
    std::wstring base = root_name(parent);
    if (base.empty()) { if (!parent) return false; base = ((Handle *)parent)->path; }
    std::wstring s = sub ? sub : L"";
    while (!s.empty() && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
    out = s.empty() ? base : base + L"\\" + lower(s);
    return true;
}

std::wstring widen(const char *s)
{
    std::wstring w; if (!s) return w;
    while (*s) w += (wchar_t)(unsigned char)*s++;
    return w;
}

/*
 * Optional import: if FX_REGISTRY_IMPORT names a file, preload it. Format, one value per line:
 *   <key path>\t<value name>\t<sz|dword>\t<data>
 * e.g. "HKCU\Software\DFX\13\23\LASTUSED_DFXP\EQ\Band12\t\tsz\t0.30" (empty name = default value).
 * Used by the null test to reproduce a Windows session state, and later for settings migration.
 */
void import_once()
{
    static bool done = false;
    if (done) return;
    done = true;
    const char *path = getenv("FX_REGISTRY_IMPORT");
    if (!path || !*path) return;
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "[fxsound] cannot open registry import %s\n", path); return; }
    char line[4096];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        size_t t1 = s.find('\t'), t2 = s.find('\t', t1 + 1), t3 = s.find('\t', t2 + 1);
        if (t1 == std::string::npos || t2 == std::string::npos || t3 == std::string::npos) continue;
        std::wstring key = widen(s.substr(0, t1).c_str()), name = widen(s.substr(t1 + 1, t2 - t1 - 1).c_str());
        std::string type = s.substr(t2 + 1, t3 - t2 - 1), data = s.substr(t3 + 1);
        std::wstring k = lower(key);
        size_t bs = k.find(L'\\');
        std::wstring root = k.substr(0, bs);
        if (root == L"hkey_current_user") root = L"hkcu";
        else if (root == L"hkey_local_machine") root = L"hklm";
        k = root + (bs == std::wstring::npos ? L"" : k.substr(bs));
        // create parent keys too, so RegEnumKeyEx sees them
        for (size_t p = k.find(L'\\'); p != std::wstring::npos; p = k.find(L'\\', p + 1)) g_keys[k.substr(0, p)];
        Value v;
        if (type == "dword") {
            DWORD d = (DWORD)strtoul(data.c_str(), nullptr, 0);
            v.type = REG_DWORD; v.data.assign((BYTE *)&d, (BYTE *)&d + sizeof d);
        } else {
            std::wstring w = widen(data.c_str());
            v.type = REG_SZ; v.data.assign((const BYTE *)w.c_str(), (const BYTE *)(w.c_str() + w.size() + 1));
        }
        g_keys[k].values[lower(name)] = v;
        n++;
    }
    fclose(f);
    if (getenv("FX_DEBUG")) fprintf(stderr, "[fxsound] imported %d registry values from %s\n", n, path);
}

struct Importer { Importer() { std::lock_guard<std::mutex> l(g_mu); import_once(); } } g_importer;

} // namespace

extern "C" LONG RegCreateKeyExW(HKEY parent, LPCWSTR sub, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY out, LPDWORD disp)
{
    std::wstring p; if (!resolve(parent, sub, p)) return ERROR_FILE_NOT_FOUND;
    std::lock_guard<std::mutex> l(g_mu);
    bool existed = g_keys.count(p) != 0;
    g_keys[p];
    if (disp) *disp = existed ? 2 /*REG_OPENED_EXISTING_KEY*/ : REG_CREATED_NEW_KEY;
    *out = (HKEY) new Handle{p};
    return ERROR_SUCCESS;
}
extern "C" LONG RegCreateKeyExA(HKEY parent, LPCSTR sub, DWORD r, LPSTR, DWORD o, REGSAM s, LPSECURITY_ATTRIBUTES a, PHKEY out, LPDWORD d)
{ return RegCreateKeyExW(parent, widen(sub).c_str(), r, nullptr, o, s, a, out, d); }

extern "C" LONG RegOpenKeyExW(HKEY parent, LPCWSTR sub, DWORD, REGSAM, PHKEY out)
{
    std::wstring p; if (!resolve(parent, sub, p)) return ERROR_FILE_NOT_FOUND;
    std::lock_guard<std::mutex> l(g_mu);
    if (!g_keys.count(p)) return ERROR_FILE_NOT_FOUND;
    *out = (HKEY) new Handle{p};
    return ERROR_SUCCESS;
}
extern "C" LONG RegOpenKeyExA(HKEY parent, LPCSTR sub, DWORD o, REGSAM s, PHKEY out)
{ return RegOpenKeyExW(parent, widen(sub).c_str(), o, s, out); }

extern "C" LONG RegCloseKey(HKEY k)
{
    if (!root_name(k).empty() || !k) return ERROR_SUCCESS;
    delete (Handle *)k;
    return ERROR_SUCCESS;
}
extern "C" LONG RegFlushKey(HKEY) { return ERROR_SUCCESS; }

extern "C" LONG RegSetValueExW(HKEY k, LPCWSTR name, DWORD, DWORD type, const BYTE *data, DWORD size)
{
    std::wstring p; if (!resolve(k, nullptr, p)) return ERROR_FILE_NOT_FOUND;
    std::lock_guard<std::mutex> l(g_mu);
    Value v; v.type = type; v.data.assign(data, data + size);
    g_keys[p].values[lower(name ? name : L"")] = v;
    return ERROR_SUCCESS;
}
extern "C" LONG RegSetValueExA(HKEY k, LPCSTR name, DWORD r, DWORD type, const BYTE *data, DWORD size)
{ return RegSetValueExW(k, widen(name).c_str(), r, type, data, size); }

extern "C" LONG RegQueryValueExW(HKEY k, LPCWSTR name, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD size)
{
    std::wstring p; if (!resolve(k, nullptr, p)) return ERROR_FILE_NOT_FOUND;
    std::lock_guard<std::mutex> l(g_mu);
    auto kit = g_keys.find(p); if (kit == g_keys.end()) return ERROR_FILE_NOT_FOUND;
    auto vit = kit->second.values.find(lower(name ? name : L""));
    if (vit == kit->second.values.end()) return ERROR_FILE_NOT_FOUND;
    const Value &v = vit->second;
    if (type) *type = v.type;
    if (!size) return ERROR_SUCCESS;
    if (data) {
        if (*size < v.data.size()) { *size = (DWORD)v.data.size(); return ERROR_MORE_DATA; }
        memcpy(data, v.data.data(), v.data.size());
    }
    *size = (DWORD)v.data.size();
    return ERROR_SUCCESS;
}
extern "C" LONG RegQueryValueExA(HKEY k, LPCSTR name, LPDWORD r, LPDWORD type, LPBYTE data, LPDWORD size)
{ return RegQueryValueExW(k, widen(name).c_str(), r, type, data, size); }

extern "C" LONG RegDeleteValueW(HKEY k, LPCWSTR name)
{
    std::wstring p; if (!resolve(k, nullptr, p)) return ERROR_FILE_NOT_FOUND;
    std::lock_guard<std::mutex> l(g_mu);
    auto kit = g_keys.find(p); if (kit == g_keys.end()) return ERROR_FILE_NOT_FOUND;
    return kit->second.values.erase(lower(name ? name : L"")) ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
}
extern "C" LONG RegDeleteValueA(HKEY k, LPCSTR name) { return RegDeleteValueW(k, widen(name).c_str()); }

extern "C" LONG RegDeleteKeyW(HKEY k, LPCWSTR sub)
{
    std::wstring p; if (!resolve(k, sub, p)) return ERROR_FILE_NOT_FOUND;
    std::lock_guard<std::mutex> l(g_mu);
    return g_keys.erase(p) ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
}
extern "C" LONG RegDeleteKeyA(HKEY k, LPCSTR sub) { return RegDeleteKeyW(k, widen(sub).c_str()); }

extern "C" LONG RegEnumKeyExW(HKEY k, DWORD index, LPWSTR name, LPDWORD name_len, LPDWORD, LPWSTR, LPDWORD, PFILETIME)
{
    std::wstring p; if (!resolve(k, nullptr, p)) return ERROR_FILE_NOT_FOUND;
    std::lock_guard<std::mutex> l(g_mu);
    std::wstring prefix = p + L"\\";
    std::vector<std::wstring> children;
    for (auto &kv : g_keys) {
        if (kv.first.compare(0, prefix.size(), prefix) != 0) continue;
        std::wstring rest = kv.first.substr(prefix.size());
        if (rest.find(L'\\') == std::wstring::npos) children.push_back(rest);
    }
    if (index >= children.size()) return ERROR_NO_MORE_ITEMS;
    if (*name_len <= children[index].size()) return ERROR_MORE_DATA;
    wcscpy(name, children[index].c_str());
    *name_len = (DWORD)children[index].size();
    return ERROR_SUCCESS;
}
extern "C" LONG RegEnumKeyExA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, PFILETIME) { return ERROR_NO_MORE_ITEMS; }

extern "C" LONG RegEnumValueW(HKEY k, DWORD index, LPWSTR name, LPDWORD name_len, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD size)
{
    std::wstring p; if (!resolve(k, nullptr, p)) return ERROR_FILE_NOT_FOUND;
    std::lock_guard<std::mutex> l(g_mu);
    auto kit = g_keys.find(p); if (kit == g_keys.end()) return ERROR_NO_MORE_ITEMS;
    if (index >= kit->second.values.size()) return ERROR_NO_MORE_ITEMS;
    auto it = kit->second.values.begin(); std::advance(it, index);
    if (*name_len <= it->first.size()) return ERROR_MORE_DATA;
    wcscpy(name, it->first.c_str()); *name_len = (DWORD)it->first.size();
    if (type) *type = it->second.type;
    if (size) {
        if (data && *size >= it->second.data.size()) memcpy(data, it->second.data.data(), it->second.data.size());
        *size = (DWORD)it->second.data.size();
    }
    return ERROR_SUCCESS;
}
extern "C" LONG RegEnumValueA(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD) { return ERROR_NO_MORE_ITEMS; }
