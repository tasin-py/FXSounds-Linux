#!/usr/bin/env python3
"""Applies the Linux port edits to upstream/fxsound/Source, starting from reference/upstream-pristine.
Every change is wrapped in JUCE_LINUX / JUCE_WINDOWS conditionals so the Windows build is unchanged."""
import os, sys, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', 'upstream', 'fxsound', 'Source')
PRISTINE = os.path.join(HERE, '..', 'reference', 'upstream-pristine', 'fxsound', 'Source')
fails = []
# Always start from the pristine upstream sources, so running this script is repeatable.
if os.path.isdir(PRISTINE):
    shutil.copytree(PRISTINE, ROOT, dirs_exist_ok=True)

def edit(rel, pairs):
    p = os.path.join(ROOT, rel)
    s = open(p, encoding='utf-8-sig').read()
    orig = s
    for item in pairs:
        old, new = item[0], item[1]
        if old not in s:
            fails.append('%s: not found: %r' % (rel, old[:80]))
            continue
        s = s.replace(old, new, 1)
    if s != orig:
        open(p, 'w', encoding='utf-8', newline='\n').write(s)

for part in ('port_gui_edits.py', 'port_gui_edits2.py'):
    exec(open(os.path.join(os.path.dirname(__file__), part), encoding='utf-8').read())

if fails:
    print('\n'.join(fails)); sys.exit(1)
print('port edits applied')
