#!/usr/bin/env python3
"""Compare a live PipeWire recording (test/out/live_N.wav) with the offline render (test/out/preset_N.wav).
Aligns by cross-correlation, then reports residual and dropouts (runs of silence inside the signal)."""
import sys, os
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from spectrum_compare import load

def main(n):
    live, sr = load('test/out/live_%s.wav' % n)
    ref, _ = load('test/out/preset_%s.wav' % n if n != 'off' else 'test/test_input.wav')
    L = live[:, 0] if live.ndim > 1 else live
    R = ref[:, 0] if ref.ndim > 1 else ref
    # coarse alignment on the first 3 s of the reference
    seg = R[:sr * 3]
    corr = np.correlate(L[:sr * 8], seg[:sr], mode='valid')
    off = int(np.argmax(np.abs(corr)))
    # refine: find first sample where live is non-silent
    nz = np.flatnonzero(np.abs(L) > 1e-6)
    first = nz[0] if len(nz) else 0
    print('live length %.2f s, signal starts at %.3f s, xcorr offset %.3f s' % (len(L) / sr, first / sr, off / sr))
    a = live[off:off + len(ref)]
    b = ref[:len(a)]
    d = a - b
    res = 10 * np.log10((d ** 2).sum() / max((b ** 2).sum(), 1e-30) + 1e-30)
    print('max |diff| %.3g, residual %.1f dB' % (np.abs(d).max(), res))
    # dropout check: 5 ms windows that are silent in live but loud in reference
    w = sr // 200
    nwin = len(b) // w
    drops = 0
    for i in range(nwin):
        if np.abs(b[i * w:(i + 1) * w]).max() > 0.01 and np.abs(a[i * w:(i + 1) * w]).max() < 1e-4:
            drops += 1
    print('dropout windows (5 ms): %d of %d' % (drops, nwin))

main(sys.argv[1] if len(sys.argv) > 1 else '8')
