"""
"chaos" style: glitchy, distorted digicore / breakcore-leaning beat.
Original material only: synthesised sounds, generated chords and melodies.

Sections (16 bars @ 160 BPM):
  1-4   intro   crushed supersaw wall fading in, high vocal chops, noise riser, tape stop
  5-12  drop    fast breakbeat, clipped 808 slides, distorted wall, lead runs, glitch fills
  13-14 switch  half-time, everything bit-crushed and decimated
  15-16 outro   double-time snare rolls, stutters, final tape stop
"""
import numpy as np

from make_beat import SR, mtof, bandlimited_table, osc, adsr, place, onepole_lp, reverb

# E major: I - V - vi - IV (E, B, C#m, A) with added colour tones
CHORDS = [
    [52, 59, 63, 66, 68],   # Eadd9
    [47, 54, 59, 61, 63],   # Badd9
    [49, 56, 61, 63, 64],   # C#m(add9)
    [45, 52, 57, 59, 61],   # Aadd9
]
ROOTS = [28, 35, 37, 33]    # E1, B1, C#2, A1
SCALE = [64, 66, 68, 71, 73, 76, 78, 80, 83, 85]   # E major pentatonic, high register

VOWELS = [(800, 1150), (400, 1600), (270, 2300), (500, 900), (330, 800)]


# ------------------------------------------------------------ fx helpers ---
def crush(x, bits):
    q = 2.0 ** (bits - 1)
    return np.round(x * q) / q


def decimate(x, factor):
    if factor <= 1:
        return x
    n = x.shape[-1]
    return np.repeat(x[..., ::factor], factor, axis=-1)[..., :n]


def dirt(x, drive, bits=None, fold=0.0):
    y = np.tanh(x * drive)
    if fold > 0:
        y = (1 - fold) * y + fold * np.sin(x * drive * 1.7)
    if bits:
        y = crush(y, bits)
    return y


def resample(x, ratio):
    """Play x at `ratio` speed (2 = octave up, shorter)."""
    n = x.shape[-1]
    out_len = max(1, int(n / ratio))
    idx = np.arange(out_len) * ratio
    if x.ndim == 1:
        return np.interp(idx, np.arange(n), x)
    return np.stack([np.interp(idx, np.arange(n), c) for c in x])


def stutter(buf, start, slice_len, repeats, pitch_up=False):
    start, slice_len = int(start), max(16, int(slice_len))
    if start + slice_len * repeats >= buf.shape[1]:
        return
    piece = buf[:, start:start + slice_len].copy()
    fade = np.minimum(1, np.minimum(np.arange(slice_len), slice_len - 1 - np.arange(slice_len)) / 40)
    for r in range(repeats):
        p = piece
        if pitch_up:
            p = resample(piece, 1 + r * 0.12)
            p = np.pad(p, ((0, 0), (0, slice_len - p.shape[1])))[:, :slice_len]
        s = start + r * slice_len
        buf[:, s:s + slice_len] = p * fade


def tape_stop(buf, start, length):
    start, length = int(start), int(length)
    if start + length >= buf.shape[1]:
        length = buf.shape[1] - start - 1
    speed = np.linspace(1, 0, length) ** 1.5
    pos = start + np.cumsum(speed)
    for c in range(2):
        buf[c, start:start + length] = np.interp(pos, np.arange(buf.shape[1]), buf[c]) * np.linspace(1, 0.3, length)


def reverse_chunk(buf, start, length):
    start, length = int(start), int(length)
    if start + length < buf.shape[1]:
        buf[:, start:start + length] = buf[:, start:start + length][:, ::-1]


# ------------------------------------------------------------- sounds ---
def kick():
    n = int(0.22 * SR)
    t = np.arange(n) / SR
    f = 50 + 190 * np.exp(-t * 45)
    y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 14)
    y[:90] += np.random.default_rng(1).standard_normal(90) * np.linspace(0.8, 0, 90)
    return dirt(y, 3.5)


def snare(rng):
    n = int(0.2 * SR)
    t = np.arange(n) / SR
    body = np.sin(2 * np.pi * 195 * t) * np.exp(-t * 30)
    noise = rng.standard_normal(n) * np.exp(-t * 18)
    noise = noise - onepole_lp(noise, 1500)
    return 0.8 * dirt(body * 0.9 + noise * 0.8, 3.0)


def hat(rng):
    n = int(0.035 * SR)
    y = rng.standard_normal(n) * np.exp(-np.arange(n) / SR * 120)
    y = y - onepole_lp(y, 8000)
    return 0.2 * y / np.max(np.abs(y))


def vox(f0, vowel, dur, rng):
    """Synth vocal chop: harmonics shaped by vowel formants + breath."""
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = f0 * (1 + 0.012 * np.sin(2 * np.pi * 6 * t))
    phase = np.cumsum(f) / SR
    F1, F2 = VOWELS[vowel]
    y = np.zeros(n)
    for k in range(1, 40):
        fk = f0 * k
        if fk > 7000:
            break
        a = np.exp(-((fk - F1) / 120) ** 2) + 0.7 * np.exp(-((fk - F2) / 170) ** 2) + 0.05 / k
        y += a * np.sin(2 * np.pi * k * phase)
    y += rng.standard_normal(n) * 0.05
    return y * adsr(n, 0.004, 0.08, 0.7, 0.03, int(n * 0.85))


def supersaw(m, n, voices=7, spread_cents=28):
    table = bandlimited_table(mtof(m), "saw")
    out = np.zeros((2, n))
    for v in range(voices):
        d = (v / (voices - 1) * 2 - 1) * spread_cents
        y = osc(np.full(n, mtof(m) * 2 ** (d / 1200)), table)
        pan = (v / (voices - 1) * 2 - 1) * 0.9
        ang = (pan + 1) * np.pi / 4
        out[0] += y * np.cos(ang)
        out[1] += y * np.sin(ang)
    return out / np.sqrt(voices)


# ------------------------------------------------------------- arrange ---
def build_chaos(bpm=160, seed=82, bars=16):
    rng = np.random.default_rng(seed)
    beat = 60.0 / bpm
    bar = 4 * beat
    step = beat / 4
    total = int((bars * bar + 2.0) * SR)
    at = lambda b, beats=0.0: int((b * bar + beats * beat) * SR)
    parts = {k: np.zeros((2, total)) for k in ("wall", "lead", "vox", "bass", "drums", "fx")}
    notes = {k: [] for k in ("chords", "melody", "808", "drums", "vox")}

    INTRO, DROP, SWITCH, OUTRO = range(0, 4), range(4, 12), range(12, 14), range(14, 16)

    # --- distorted supersaw wall (shoegaze-ish) ---
    for b in range(0, bars, 2):
        chord = CHORDS[(b // 2) % 4]
        n = int((2 * bar + 0.4) * SR)
        env = adsr(n, 0.02, 0.6, 0.85, 0.3, int(2 * bar * SR))
        layer = np.zeros((2, n))
        for m in chord:
            layer += supersaw(m, n)
            notes["chords"].append((b * 4, 8, m, 100))
        layer = dirt(layer * 0.9, 4.0, bits=None, fold=0.25) * env
        layer = onepole_lp(layer, 7000)
        parts["wall"][:, at(b):at(b) + n] += layer[:, : max(0, min(n, total - at(b)))] * 0.22

    # --- lead: fast pentatonic runs + held bends, retriggered glitches ---
    for b in list(DROP) + list(OUTRO):
        pos = 0.0
        while pos < 4:
            run = rng.random() < 0.55
            ln = 0.25 if run else rng.choice([0.5, 0.75, 1.0])
            deg = int(rng.integers(0, len(SCALE)))
            m = SCALE[deg]
            vel = int(85 + 40 * rng.random()) if run else 120
            notes["melody"].append((b * 4 + pos, ln * 0.9, m, min(127, vel)))
            n = int((ln * beat + 0.05) * SR)
            t = np.arange(n) / SR
            bend = 1 if run else 2 ** ((-1.5 * np.exp(-t * 25)) / 12)        # scoop up into long notes
            f = mtof(m) * bend * (1 + 0.006 * np.sin(2 * np.pi * 7 * t))
            y = osc(f, bandlimited_table(mtof(m), "saw")) + 0.5 * osc(f * 2.003, bandlimited_table(mtof(m) * 2, "saw"))
            y = dirt(y, 3.0, bits=10) * adsr(n, 0.002, 0.15, 0.6, 0.03, int(ln * beat * SR * 0.9))
            pan = rng.uniform(-0.4, 0.4)
            place(parts["lead"], y * 0.13 * vel / 127, at(b, pos), pan)
            pos += ln

    # --- high vocal chops (synthesised, pitched up) ---
    for b in list(INTRO) + list(DROP):
        for s in range(16):
            if rng.random() < (0.35 if b in INTRO else 0.25):
                m = int(rng.choice(SCALE[:6])) + 12
                vowel = int(rng.integers(0, 5))
                dur = step * rng.choice([1, 1, 2])
                y = vox(mtof(m - 12), vowel, dur, rng)
                y = resample(y, 2.0 ** 1.0)                              # chipmunk pitch-up
                y = dirt(y * 1.5, 2.0, bits=9)
                place(parts["vox"], y * 0.12, at(b, s / 4), rng.uniform(-0.6, 0.6))
                notes["vox"].append((b * 4 + s / 4, dur / beat, m, 100))

    # --- drums ---
    k, sn, hats = kick(), snare(rng), [hat(rng) for _ in range(3)]
    breaks = [
        "K.....K...K.....", "....S..s.s..S..s",   # kick / snare (s = ghost)
    ]
    for b in range(bars):
        if b in INTRO:
            continue
        half = b in SWITCH
        kicks = [i for i, c in enumerate(breaks[0]) if c == "K"]
        snares = [(i, c) for i, c in enumerate(breaks[1]) if c in "Ss"]
        if half:
            kicks, snares = [0, 7, 10], [(8, "S")]
        if rng.random() < 0.4 and not half:
            kicks.append(int(rng.choice([3, 13, 14])))
        for i in kicks:
            place(parts["drums"], k * 1.0, at(b, i / 4))
            notes["drums"].append((b * 4 + i / 4, 0.25, 36, 120))
        for i, c in snares:
            v = 1.0 if c == "S" else 0.35
            place(parts["drums"], sn * v, at(b, i / 4), 0.05)
            notes["drums"].append((b * 4 + i / 4, 0.25, 38, int(127 * v)))
        hat_steps = range(0, 16, 2 if half else 1)
        for i in hat_steps:
            v = 0.5 + 0.5 * (i % 2 == 0) * rng.random()
            place(parts["drums"], hats[i % 3] * v, at(b, i / 4), 0.35 * (1 if i % 2 else -1))
            notes["drums"].append((b * 4 + i / 4, 0.1, 42, int(60 + 60 * v)))
        if b in OUTRO or b % 4 == 3:                                     # snare roll, rising
            roll_start = 2.0 if b in OUTRO else 3.0
            count = 16 if b in OUTRO else 8
            for r in range(count):
                pos = roll_start + r * (4 - roll_start) / count
                place(parts["drums"], resample(sn, 1 + r / count) * (0.3 + 0.7 * r / count), at(b, pos))
                notes["drums"].append((b * 4 + pos, 0.05, 38, int(50 + 70 * r / count)))

    # --- 808: clipped and folded, slides, octave jumps ---
    pattern = [(0, 1.5), (1.75, 0.5), (2.5, 1.0), (3.5, 0.5)]
    for b in range(bars):
        if b in INTRO:
            continue
        root = ROOTS[(b // 2) % 4]
        prev = root
        pat = [(0, 2.0), (2.5, 1.5)] if b in SWITCH else pattern
        for j, (pos, ln) in enumerate(pat):
            m = root + (12 if rng.random() < 0.3 and j > 0 else 0)
            notes["808"].append((b * 4 + pos, ln, m, 120))
            n = int((ln * beat + 0.05) * SR)
            t = np.arange(n) / SR
            f = mtof(prev) + (mtof(m) - mtof(prev)) * (1 - np.exp(-t * 30))
            f = f * (1 + 0.8 * np.exp(-t * 70))
            y = np.sin(2 * np.pi * np.cumsum(f) / SR)
            y = dirt(y, 6.0, fold=0.3) * adsr(n, 0.001, 1.0, 0.8, 0.04, int(ln * beat * SR))
            place(parts["bass"], y * 0.85, at(b, pos))
            prev = m

    # --- fx: noise riser into the drop, impact crash ---
    rlen = at(4) - at(2)
    riser = rng.standard_normal((2, rlen)) * np.linspace(0, 1, rlen) ** 2
    riser = riser - onepole_lp(riser, 2500)
    parts["fx"][:, at(2):at(4)] += riser * 0.18
    for b in (4, 12, 14):
        n = int(1.5 * SR)
        crash = rng.standard_normal((2, n)) * np.exp(-np.arange(n) / SR * 3)
        parts["fx"][:, at(b):at(b) + n] += (crash - onepole_lp(crash, 4000)) * 0.2

    # --------------------------------------------------------- mixing ---
    kick_env = np.zeros(total)
    for st, _, nt, _ in notes["drums"]:
        if nt == 36:
            s = int(st * beat * SR)
            d = np.exp(-np.arange(int(0.18 * SR)) / SR * 16)
            e = min(total, s + len(d))
            kick_env[s:e] = np.maximum(kick_env[s:e], d[: e - s])
    pump = 1 - 0.65 * kick_env

    intro_end = at(4)
    sweep = np.ones(total)
    sweep[:intro_end] = np.linspace(0.05, 1, intro_end) ** 2
    wall = parts["wall"]
    wall = onepole_lp(wall, 600) * (1 - sweep) + wall * sweep
    music = (wall + parts["lead"] + parts["vox"]) * pump
    wet = reverb(music * 0.7, seconds=2.2)
    bus = music + wet * 0.8 + parts["bass"] * pump ** 0.3 + parts["drums"] + parts["fx"]

    # switch section: everything crushed
    s0, s1 = at(12), at(14)
    bus[:, s0:s1] = decimate(crush(np.tanh(bus[:, s0:s1] * 2), 6), 4)

    # glitch fills on the master (seeded so each seed glitches differently)
    for b in range(5, bars, 2):
        kind = rng.integers(0, 3)
        if kind == 0:
            stutter(bus, at(b, 3), step, 4)
        elif kind == 1:
            stutter(bus, at(b, 3.5), step / 2, 4, pitch_up=True)
        else:
            reverse_chunk(bus, at(b, 3), beat)
    tape_stop(bus, at(3, 3), beat)            # into the drop
    bus[:, at(3, 4) - int(step * SR):at(4)] *= 0.0   # tiny silence before the hit
    tape_stop(bus, at(11, 3), beat)           # into the switch
    tape_stop(bus, at(15, 2.5), 1.5 * beat)   # ending

    # master: tame the fizz, then drive + limiter-ish
    bus = onepole_lp(bus, 9000) * 0.75 + bus * 0.25
    bus = np.tanh(bus * 2.2) / np.tanh(2.2)
    bus *= 0.93 / (np.max(np.abs(bus)) + 1e-9)
    return bus, notes
