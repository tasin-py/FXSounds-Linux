#!/usr/bin/env python3
"""
convert_long.py - make the FxSound DSP sources independent of the size of `long`.

The DSP was written for Windows, where `long` is 32 bits on both x86 and x64 (LLP64).
On 64-bit Linux (LP64) `long` is 64 bits, which breaks the engine's parameter memory
(floats are written through `long *`, arrays of `long` are indexed as 4-byte words, ...).

This script rewrites, in code only (never in comments or string literals):
    unsigned long [int]  -> pt_ulong
    signed long [int]    -> pt_long
    long [int]           -> pt_long
leaving `long long` and `long double` alone, and in string literals rewrites printf/scanf
`%ld/%lu/%lx/%li` to `%d/%u/%x/%i` to match. pt_long/pt_ulong are 32-bit typedefs from
compat/fx_linux_compat.h, i.e. exactly what upstream gets on Windows x64.

Usage: python3 tools/convert_long.py FILE...    (edits in place, prints a summary)
It is idempotent.
"""
import re
import sys

KEEP = [r'\bunsigned\s+long\s+long\b', r'\blong\s+long\b', r'\blong\s+double\b']
RULES = [
    (re.compile(r'\bunsigned\s+long(\s+int)?\b'), 'pt_ulong'),
    (re.compile(r'\blong\s+unsigned(\s+int)?\b'), 'pt_ulong'),
    (re.compile(r'\bsigned\s+long(\s+int)?\b'), 'pt_long'),
    (re.compile(r'\blong(\s+int)?\b'), 'pt_long'),
]
FMT = re.compile(r'%([-+ #0]*[0-9*]*(?:\.[0-9*]+)?)l([diuxX])')


def convert_code(code):
    # protect the multi-word types we keep
    saved = []
    def protect(m):
        saved.append(m.group(0))
        return '\x00%d\x00' % (len(saved) - 1)
    for k in KEEP:
        code = re.sub(k, protect, code)
    n = 0
    for rx, rep in RULES:
        code, c = rx.subn(rep, code)
        n += c
    code = re.sub('\x00(\\d+)\x00', lambda m: saved[int(m.group(1))], code)
    return code, n


def convert(text):
    out = []
    i, n = 0, len(text)
    code_start = 0
    total = 0
    fmt_total = 0

    def flush_code(end):
        nonlocal total
        seg, c = convert_code(text[code_start:end])
        total += c
        out.append(seg)

    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if ch == '/' and nxt == '/':
            flush_code(i)
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(text[i:j]); i = j; code_start = i
        elif ch == '/' and nxt == '*':
            flush_code(i)
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(text[i:j]); i = j; code_start = i
        elif ch == '"' or ch == "'":
            flush_code(i)
            j = i + 1
            while j < n and text[j] != ch:
                if text[j] == '\\':
                    j += 1
                elif text[j] == '\n':
                    break
                j += 1
            j = min(j + 1, n)
            lit = text[i:j]
            if ch == '"':
                lit, c = FMT.subn(r'%\1\2', lit)
                fmt_total += c
            out.append(lit); i = j; code_start = i
        else:
            i += 1
    flush_code(n)
    return ''.join(out), total, fmt_total


def main(paths):
    grand = 0
    for p in paths:
        with open(p, 'r', encoding='utf-8', errors='surrogateescape', newline='') as f:
            text = f.read()
        new, c, fc = convert(text)
        if new != text:
            with open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='') as f:
                f.write(new)
            print('%5d types %3d formats  %s' % (c, fc, p))
            grand += c
    print('total: %d replacements' % grand)


if __name__ == '__main__':
    main(sys.argv[1:])
