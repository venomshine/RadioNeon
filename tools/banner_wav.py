# Radio Neon - HOME menu banner sound: meta/banner_src.wav (the original sound) -> meta/banner.wav
# The HOME Menu plays the banner at the DSP rate; anything else gets resampled there and sounds bad.
# So the original is converted here once, with a good resampler: stereo PCM16 at 32728 Hz.
# Pure Python, no dependencies:  python tools/banner_wav.py
import math, struct, wave

SR = 32728
TAPS = 32          # kernel half-width in input samples
BETA = 8.6         # Kaiser window

def i0(x):
    s, t, k = 1.0, 1.0, 1
    while t > 1e-12 * s:
        t *= (x / (2 * k)) ** 2
        s += t
        k += 1
    return s

with wave.open("meta/banner_src.wav", "rb") as w:
    ch, bits, sr_in, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
assert bits == 2, "banner_src.wav must be PCM16"
smp = struct.unpack("<%dh" % (n * ch), raw)
chans = [[smp[i * ch + c] / 32768.0 for i in range(n)] for c in range(ch)]

ratio = SR / sr_in
fc = 0.5 * min(1.0, ratio) * 0.97          # cutoff relative to the input rate
norm = i0(BETA)
out_n = int(n * ratio)
res = []
for x in chans:
    y = []
    for j in range(out_n):
        t = j / ratio
        c = int(math.floor(t))
        acc = 0.0
        for k in range(c - TAPS + 1, c + TAPS + 1):
            if 0 <= k < n:
                d = t - k
                r = d / TAPS
                if abs(r) < 1:
                    s = 2 * fc * (math.sin(math.pi * 2 * fc * d) / (math.pi * 2 * fc * d) if d else 1.0)
                    acc += x[k] * s * i0(BETA * math.sqrt(1 - r * r)) / norm
        y.append(acc)
    res.append(y)
if len(res) == 1:
    res.append(res[0])                      # mono -> same signal on both sides
peak = max(max(abs(v) for v in y) for y in res)
g = 0.999 / peak if peak > 0.999 else 1.0  # never clip
frames = b"".join(struct.pack("<hh", *(max(-32768, min(32767, int(round(y[j] * g * 32767)))) for y in res[:2]))
                  for j in range(out_n))
with wave.open("meta/banner.wav", "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(frames)
print("meta/banner.wav: %d Hz stereo, %.2f s (from %d Hz, %d ch%s)" % (SR, out_n / SR, sr_in, ch,
      ", gain %.3f" % g if g != 1.0 else ""))
