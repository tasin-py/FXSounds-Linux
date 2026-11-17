#!/usr/bin/env python3
"""Compare octave-band energy of processed files against test/out/off.wav (bypass).
Usage: python test/spectrum_compare.py test/out/*.wav"""
import struct, sys, wave
import numpy as np


def load(path):
    with open(path, 'rb') as f:
        data = f.read()
    # minimal RIFF parse (fxprocess writes float32; test input is int16)
    pos, fmt, ch, sr, bits = 12, None, 0, 0, 0
    while pos < len(data):
        cid, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            fmt, ch, sr = struct.unpack('<HHI', body[:8]); bits = struct.unpack('<H', body[14:16])[0]
        elif cid == b'data':
            if fmt == 3: x = np.frombuffer(body, '<f4')
            else: x = np.frombuffer(body, '<i2') / 32768.0
            return x.reshape(-1, ch).astype(np.float64), sr
        pos += 8 + size + (size & 1)
    raise ValueError(path)


BANDS = [31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]


def band_db(x, sr):
    m = x.mean(axis=1)
    spec = np.abs(np.fft.rfft(m * np.hanning(len(m)))) ** 2
    freqs = np.fft.rfftfreq(len(m), 1 / sr)
    out = []
    for c in BANDS:
        sel = (freqs >= c / 2 ** 0.5) & (freqs < c * 2 ** 0.5)
        out.append(10 * np.log10(spec[sel].sum() + 1e-20))
    return np.array(out)


if __name__ == '__main__':
    ref, sr = load('test/out/off.wav')
    ref_db = band_db(ref, sr)
    print('%-24s' % 'dB vs bypass' + ''.join('%7s' % (('%dk' % (b // 1000)) if b >= 1000 else b) for b in BANDS) + '   width')
    for p in sys.argv[1:]:
        if p.endswith('off.wav'): continue
        x, _ = load(p)
        d = band_db(x, sr) - ref_db
        side = (x[:, 0] - x[:, 1]); mid = (x[:, 0] + x[:, 1])
        width = 10 * np.log10((side ** 2).sum() / (mid ** 2).sum())
        print('%-24s' % p.split('/')[-1] + ''.join('%7.1f' % v for v in d) + '  %6.1f' % width)
    rs = ref[:, 0] - ref[:, 1]; rm = ref[:, 0] + ref[:, 1]
    print('%-24s' % 'bypass width (S/M dB)' + ' ' * 70 + '%6.1f' % (10 * np.log10((rs ** 2).sum() / (rm ** 2).sum())))
