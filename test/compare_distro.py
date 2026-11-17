#!/usr/bin/env python3
"""Compare the live recordings of test/distro_test.sh (test/out/distro_<label>.wav) with the offline render of
the fresh-install preset (test/out/ref_general_gui.wav = fxprocess test_input.wav --preset 1.fac --gui-init).
  python3 test/compare_distro.py [label ...]"""
import sys, os, glob
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from spectrum_compare import load


def compare(path, ref, sr):
    live, _ = load(path)
    L = live[:, 0] if live.ndim > 1 else live
    R = ref[:, 0] if ref.ndim > 1 else ref
    corr = np.correlate(L[:sr * 8], R[:sr], mode='valid')
    off = int(np.argmax(np.abs(corr)))
    a = live[off:off + len(ref)]
    b = ref[:len(a)]
    d = a - b
    res = 10 * np.log10((d ** 2).sum() / max((b ** 2).sum(), 1e-30) + 1e-30)
    w = sr // 200
    bb = b[:, 0] if b.ndim > 1 else b
    aa = a[:, 0] if a.ndim > 1 else a
    drops = sum(1 for i in range(len(bb) // w)
                if np.abs(bb[i * w:(i + 1) * w]).max() > 1e-3 and np.abs(aa[i * w:(i + 1) * w]).max() < 1e-6)
    cover = len(a) / len(ref)
    # < 1e-12 (-240 dBFS): float rounding only (e.g. 2.3e-26 from a different number of silent blocks processed
    # before the signal); anything audible is many orders of magnitude larger
    ok = np.abs(d).max() < 1e-12 and drops == 0 and cover > 0.999
    return ok, 'max|diff| %.3g  residual %.1f dB  dropouts %d  coverage %.1f%%' % (np.abs(d).max(), res, drops, 100 * cover)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ref, sr = load(os.path.join(here, 'out', 'ref_general_gui.wav'))
    labels = sys.argv[1:] or sorted(os.path.basename(p)[7:-4] for p in glob.glob(os.path.join(here, 'out', 'distro_*.wav')))
    bad = 0
    for lab in labels:
        ok, msg = compare(os.path.join(here, 'out', 'distro_%s.wav' % lab), ref, sr)
        bad += not ok
        print('%-12s %s  %s' % (lab, 'IDENTICAL' if ok else 'DIFFERS', msg))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
