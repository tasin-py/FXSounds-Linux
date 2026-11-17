#!/usr/bin/env python3
"""Null test: Windows reference (pristine upstream, MSVC) vs Linux port.
Runs build-win/fxprocess.exe for the same cases as test/process_all.sh, then compares with
test/out/*.wav (fetched from WSL). Prints max abs difference and residual level vs signal.
Run from the project root on Windows:  python test/null_test.py"""
import glob, os, subprocess, sys
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from spectrum_compare import load  # noqa: E402  (module prints nothing on import? see guard below)

EXE = os.path.join('build-win', 'fxprocess.exe')
os.makedirs('test/out-win', exist_ok=True)
# sliders5 has no preset, so it depends on the DSP's registry session state; the Linux side must be
# run with FX_REGISTRY_IMPORT=test/fixtures/windows_session.tsv (test/sliders5_with_session.sh).
cases = [('off.wav', ['--off']),
         ('sliders5.wav', ['--fidelity', '5', '--ambience', '5', '--surround', '5', '--dynamic', '5', '--bass', '5'])]
for p in sorted(glob.glob('upstream/presets/*.fac')):
    n = os.path.splitext(os.path.basename(p))[0]
    if n == 'Default':
        continue
    cases.append(('preset_%s.wav' % n, ['--preset', p]))

worst = -999.0
print('%-18s %12s %14s %10s' % ('case', 'max |diff|', 'residual dB', 'verdict'))
for out, args in cases:
    w = os.path.join('test/out-win', out)
    if os.path.exists(w):
        os.remove(w)
    # The pristine upstream DSP can crash in its destructor (uninitialized preset_list_handle_,
    # fixed in the Linux port) AFTER the output is written, so don't trust the exit code.
    r = subprocess.run([EXE, 'test/test_input.wav', w] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not os.path.exists(w):
        print('%-18s Windows reference failed (exit %d)' % (out, r.returncode)); continue
    crashed_on_exit = r.returncode != 0
    lin = os.path.join('test/out', out)
    if not os.path.exists(lin):
        print('%-18s missing Linux output' % out); continue
    a, _ = load(w); b, _ = load(lin)
    n = min(len(a), len(b))
    d = a[:n] - b[:n]
    mx = np.abs(d).max()
    res = 10 * np.log10((d ** 2).sum() / max((a[:n] ** 2).sum(), 1e-30) + 1e-30)
    worst = max(worst, res)
    verdict = 'IDENTICAL' if mx == 0 else ('match' if res < -100 else ('close' if res < -60 else 'DIFFERENT'))
    print('%-18s %12.3g %14.1f %10s%s' % (out, mx, res, verdict, '  (reference crashed on exit)' if crashed_on_exit else ''))
print('worst residual: %.1f dB relative to signal' % worst)
