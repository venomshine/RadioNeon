# Radio Neon - HOME menu banner sound -> meta/banner.wav
# The HOME speaker makes pure sines/squares sound like a phone ring, so this is built like the
# Gemi3DS one: FM bells with inharmonic partials, a short "tuning" sweep (filtered noise) first,
# cross-fed stereo echo and a fade out. Stereo PCM16 at 32728 Hz (DSP rate: no resampling), < 3 s.
# Pure Python, no dependencies:  python tools/gen_banner_wav.py
import math
import random
import struct
import wave

SR = 32728
DUR = 2.6
N = int(SR * DUR)
random.seed(3)

L = [0.0] * N
R = [0.0] * N


def add(buf, i, v):
    if 0 <= i < N:
        buf[i] += v


# 1) tuning sweep: band-passed noise whose centre glides up, like turning a dial onto a station
lp1 = lp2 = 0.0
for i in range(int(0.32 * SR)):
    t = i / SR
    fc = 600 + 2600 * (t / 0.32) ** 1.5
    a = 1 - math.exp(-2 * math.pi * fc / SR)
    n = random.uniform(-1, 1)
    lp1 += (n - lp1) * a
    lp2 += (lp1 - lp2) * a
    bp = lp1 - lp2
    env = math.sin(math.pi * t / 0.32) ** 2
    v = bp * env * 0.55
    pan = t / 0.32                       # sweeps left -> right
    add(L, i, v * (1 - 0.6 * pan))
    add(R, i, v * (0.4 + 0.6 * pan))


# 2) FM bells (carrier + inharmonic modulator, index decays -> bright attack, soft tail)
def bell(t0, f, amp, pan, dur=1.8, ratio=3.5, idx=2.4):
    s = int(t0 * SR)
    for k in range(int(dur * SR)):
        t = k / SR
        env = math.exp(-t * 3.2) * min(1.0, t / 0.004)
        I = idx * math.exp(-t * 5.0)
        v = math.sin(2 * math.pi * f * t + I * math.sin(2 * math.pi * f * ratio * t))
        v += 0.35 * math.sin(2 * math.pi * f * 2.76 * t) * math.exp(-t * 6.0)   # inharmonic partial
        v += 0.18 * math.sin(2 * math.pi * f * 5.40 * t) * math.exp(-t * 9.0)
        v *= env * amp
        add(L, s + k, v * (1 - pan))
        add(R, s + k, v * pan)


bell(0.30, 659.26, 0.42, 0.40)   # E5
bell(0.48, 987.77, 0.34, 0.65)   # B5
bell(0.66, 1318.5, 0.22, 0.55, dur=1.4, ratio=2.9)   # E6 sparkle

# 3) cross-fed echo (ping-pong)
d = int(0.16 * SR)
for i in range(d, N):
    L[i] += 0.32 * R[i - d]
    R[i] += 0.32 * L[i - d]

# 4) fade out + normalise to -3 dBFS
fade = int(0.5 * SR)
for i in range(N - fade, N):
    g = (N - i) / fade
    L[i] *= g
    R[i] *= g
peak = max(max(abs(x) for x in L), max(abs(x) for x in R))
g = 0.707 * 32767 / peak
with wave.open("meta/banner.wav", "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(b"".join(struct.pack("<hh", int(L[i] * g), int(R[i] * g)) for i in range(N)))
print("meta/banner.wav: %d Hz stereo, %.2f s" % (SR, DUR))
