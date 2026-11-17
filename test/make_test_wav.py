#!/usr/bin/env python3
"""Generate test/test_input.wav: 12 s, 48 kHz stereo, 16-bit.
0-3 s log sweep 20 Hz-20 kHz (L) / reversed (R); 3-6 s pink-ish noise; 6-9 s kick+snare pattern;
9-12 s A-minor chord with stereo panning. Peak ~ -6 dBFS so effects have headroom to boost."""
import math, random, struct, sys, wave

SR, DUR = 48000, 12.0
N = int(SR * DUR)
random.seed(1)
L = [0.0] * N
R = [0.0] * N

# 1) log sweeps
f0, f1, T = 20.0, 20000.0, 3.0
k = math.log(f1 / f0)
for i in range(int(SR * T)):
    t = i / SR
    ph = 2 * math.pi * f0 * T / k * (math.exp(t / T * k) - 1)
    tr = T - t
    phr = 2 * math.pi * f0 * T / k * (math.exp(tr / T * k) - 1)
    L[i] = 0.4 * math.sin(ph)
    R[i] = 0.4 * math.sin(phr)

# 2) pink-ish noise (Paul Kellet filter)
b = [0.0] * 7
for i in range(int(SR * 3), int(SR * 6)):
    w = random.uniform(-1, 1)
    b[0] = 0.99886 * b[0] + w * 0.0555179; b[1] = 0.99332 * b[1] + w * 0.0750759
    b[2] = 0.96900 * b[2] + w * 0.1538520; b[3] = 0.86650 * b[3] + w * 0.3104856
    b[4] = 0.55000 * b[4] + w * 0.5329522; b[5] = -0.7616 * b[5] - w * 0.0168980
    p = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362) * 0.06
    b[6] = w * 0.115926
    L[i] = p; R[i] = p * 0.9 + random.uniform(-0.02, 0.02)

# 3) kick + snare, 120 bpm
for beat in range(6):
    start = int(SR * (6 + beat * 0.5))
    if beat % 2 == 0:  # kick
        for j in range(int(SR * 0.3)):
            t = j / SR
            v = 0.5 * math.sin(2 * math.pi * (50 + 100 * math.exp(-t * 30)) * t) * math.exp(-t * 8)
            L[start + j] += v; R[start + j] += v
    else:              # snare
        for j in range(int(SR * 0.2)):
            t = j / SR
            v = (0.25 * random.uniform(-1, 1) + 0.15 * math.sin(2 * math.pi * 190 * t)) * math.exp(-t * 18)
            L[start + j] += v; R[start + j] += v

# 4) A-minor chord, panned voices
for idx, f in enumerate([220.0, 261.63, 329.63, 440.0]):
    pan = idx / 3.0
    for i in range(int(SR * 9), N):
        t = (i - SR * 9) / SR
        env = min(1.0, t * 20) * math.exp(-t * 0.4)
        v = 0.12 * env * (math.sin(2 * math.pi * f * t) + 0.3 * math.sin(4 * math.pi * f * t))
        L[i] += v * (1 - pan); R[i] += v * pan

out = sys.argv[1] if len(sys.argv) > 1 else 'test/test_input.wav'
with wave.open(out, 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    frames = bytearray()
    for l, r in zip(L, R):
        frames += struct.pack('<hh', int(max(-1, min(1, l)) * 32767), int(max(-1, min(1, r)) * 32767))
    w.writeframes(bytes(frames))
print('wrote', out)
