"""
"rage" style: maximal, blown-out rage / digicore with a rave switch-up.
Original material only (own chords, melodies, patterns, synthesised sounds).

24 bars @ 152 BPM, F minor:
  1-4    intro    filtered gated synth + vocal-chop hook, riser, tape stop
  5-12   drop A   half-time rage trap: gated supersaw stabs, gliding lead, blown-out 808, hat rolls, growl fills
  13-14  break    drums out, washed-out chords, chops, snare build
  15-22  drop B   four-on-the-floor rave: kick every beat, offbeat hats, arp bleeps, lead an octave up
  23-24  outro    crushed, stutters, final tape stop
"""
import numpy as np

from make_beat import SR, mtof, bandlimited_table, osc, adsr, place, onepole_lp, reverb
from chaos import crush, decimate, dirt, resample, stutter, tape_stop, reverse_chunk, vox, supersaw, snare, hat

CHORDS = [
    [53, 56, 60, 63, 67],   # Fm9
    [49, 53, 56, 60, 63],   # Dbmaj9
    [46, 53, 56, 61, 65],   # Bbm7
    [48, 52, 55, 58, 61],   # C7(b9)
]
ROOTS = [29, 25, 34, 24]                      # F1, Db1, Bb1, C1
SCALE = [65, 68, 70, 72, 75, 77, 80, 82, 84]  # F minor pentatonic
GATES = ["x..x..x.x..x.xx.", "x.xx..x...x.x..x", "x..x.xx.x..x..xx", "xx.x..x.x.x.xxxx"]


def hard_kick():
    n = int(0.3 * SR)
    t = np.arange(n) / SR
    f = 48 + 220 * np.exp(-t * 50)
    y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 9)
    y[:120] += np.random.default_rng(2).standard_normal(120) * np.linspace(1, 0, 120)
    return np.clip(y * 4, -1, 1)


def clap(rng):
    n = int(0.22 * SR)
    t = np.arange(n) / SR
    env = sum((t >= o) * np.exp(-np.maximum(t - o, 0) * 80) for o in (0, 0.009, 0.019)) + 0.5 * np.exp(-t * 16)
    y = rng.standard_normal(n) * env
    y = y - onepole_lp(y, 1100)
    return dirt(y / np.max(np.abs(y)), 2.5) * 0.75


def open_hat(rng):
    n = int(0.16 * SR)
    y = rng.standard_normal(n) * np.exp(-np.arange(n) / SR * 18)
    y = y - onepole_lp(y, 7000)
    return 0.22 * y / np.max(np.abs(y))


def glide_lead(seq, beat, octave=0):
    """Mono lead with portamento. seq = [(start_beat, len_beat, midi)], returns mono audio."""
    total = int((seq[-1][0] + seq[-1][1]) * beat * SR) + int(0.1 * SR)
    freq = np.zeros(total)
    amp = np.zeros(total)
    prev = seq[0][2] + octave
    for start, ln, m in seq:
        m += octave
        s, n = int(start * beat * SR), int(ln * beat * SR)
        t = np.arange(n) / SR
        midi = m + (prev - m) * np.exp(-t * 45) + 0.18 * np.sin(2 * np.pi * 6.5 * t) * np.minimum(t * 4, 1)
        freq[s:s + n] = mtof(midi)
        amp[s:s + n] = np.maximum(amp[s:s + n], adsr(n, 0.003, 0.2, 0.75, 0.02, int(n * 0.92)))
        prev = m
    freq[freq == 0] = mtof(prev)
    top = mtof(max(m for _, _, m in seq) + octave)
    y = osc(freq, bandlimited_table(top, "saw")) + 0.6 * osc(freq * 1.007, bandlimited_table(top, "saw")) \
        + 0.4 * osc(freq * 0.5, bandlimited_table(top, "saw"))
    return dirt(y * amp, 5.0, fold=0.2)


def growl(m, n):
    """FM growl bass stab."""
    t = np.arange(n) / SR
    f = mtof(m)
    index = 6 + 5 * np.sin(2 * np.pi * 9 * t)
    y = np.sin(2 * np.pi * f * t + index * np.sin(2 * np.pi * f * 1.5 * t))
    form = onepole_lp(y, 2500) - onepole_lp(y, 300)
    return dirt(form * 3 + y * 0.5, 4.0, bits=7) * adsr(n, 0.002, 0.3, 0.8, 0.02, int(n * 0.9))


def ott(x):
    """Rough multiband squash: every band pushed toward full level."""
    low = onepole_lp(x, 140)
    high = x - onepole_lp(x, 4500)
    mid = x - low - high
    out = np.tanh(low * 2.6) * 0.75 + np.tanh(mid * 3.2) * 0.6 + np.tanh(high * 3.0) * 0.3
    return np.clip(out * 1.5, -1, 1)          # hard clip = the blown-out sound


def build_rage(bpm=152, seed=82, bars=24):
    rng = np.random.default_rng(seed)
    beat = 60.0 / bpm
    bar = 4 * beat
    step = beat / 4
    total = int((bars * bar + 1.5) * SR)
    at = lambda b, beats=0.0: int((b * bar + beats * beat) * SR)
    P = {k: np.zeros((2, total)) for k in ("stab", "lead", "arp", "vox", "bass", "drums", "fx")}
    notes = {k: [] for k in ("chords", "melody", "808", "drums", "vox", "arp")}

    INTRO, A, BREAK, B, OUTRO = range(0, 4), range(4, 12), range(12, 14), range(14, 22), range(22, 24)
    drop = list(A) + list(B) + list(OUTRO)

    # --- gated supersaw stabs (one chord per bar) ---
    for b in range(bars):
        chord = CHORDS[b % 4]
        n = int(bar * SR)
        layer = sum(supersaw(m, n, voices=7, spread_cents=35) for m in chord)
        layer += supersaw(chord[0] - 12, n, voices=3, spread_cents=15)
        gate = np.zeros(n)
        if b in BREAK:
            gate[:] = 1.0
            notes["chords"] += [(b * 4, 4, m, 90) for m in chord]
        else:
            pat = GATES[(b + seed) % 4]
            for i, c in enumerate(pat):
                if c == "x":
                    s, ln = int(i * step * SR), int(step * SR * 0.8)
                    gate[s:s + ln] = adsr(ln, 0.002, 0.05, 0.8, 0.01, int(ln * 0.85))[: max(0, min(ln, n - s))]
                    notes["chords"] += [(b * 4 + i / 4, 0.2, m, 110) for m in chord]
        layer = dirt(layer * 0.8, 5.0, fold=0.3) * gate
        P["stab"][:, at(b):at(b) + n] += layer[:, : total - at(b)] * 0.2

    # --- gliding lead: 2-bar phrases, question / answer ---
    def phrase():
        seq, pos = [], 0.0
        while pos < 8:
            ln = float(rng.choice([0.25, 0.5, 0.5, 0.75, 1.0, 1.5]))
            ln = min(ln, 8 - pos)
            seq.append((pos, ln, int(rng.choice(SCALE[2:8]))))
            pos += ln
        return seq
    q, a = phrase(), phrase()
    for b in list(A)[::2] + list(B)[::2]:
        seq = q if (b // 2) % 2 == 0 else a
        octave = 12 if b in B else 0
        y = glide_lead(seq, beat, octave)
        place(P["lead"], y * 0.2, at(b), 0.1)
        notes["melody"] += [(b * 4 + s, ln, m + octave, 115) for s, ln, m in seq]

    # --- vocal-chop hook: one 1-bar pattern that repeats and follows the chords ---
    hook = [(i / 4, int(rng.integers(0, 5)), int(rng.integers(1, 4))) for i in sorted(rng.choice(16, 7, replace=False))]
    for b in list(INTRO) + list(A) + list(BREAK) + list(B)[4:]:
        chord = CHORDS[b % 4]
        for pos, vowel, tone in hook:
            m = chord[tone] + 24
            y = resample(vox(mtof(m - 12), vowel, step * 1.6, rng), 2.0)
            y = dirt(y * 1.5, 2.5, bits=8)
            place(P["vox"], y * 0.13, at(b, pos), rng.uniform(-0.5, 0.5))
            notes["vox"].append((b * 4 + pos, 0.2, m, 105))

    # --- arp bleeps in drop B ---
    for b in B:
        chord = CHORDS[b % 4]
        for i in range(16):
            m = chord[[0, 2, 4, 3, 1, 4, 2, 3][i % 8]] + 24 + (12 if i % 8 == 5 else 0)
            n = int(step * SR)
            y = np.sign(np.sin(2 * np.pi * mtof(m) * np.arange(n) / SR)) * np.exp(-np.arange(n) / SR * 22)
            place(P["arp"], crush(y, 5) * 0.045, at(b, i / 4), 0.6 * (1 if i % 2 else -1))
            notes["arp"].append((b * 4 + i / 4, 0.2, m, 90))

    # --- drums ---
    k, cl, sn, oh = hard_kick(), clap(rng), snare(rng), open_hat(rng)
    hats = [hat(rng) for _ in range(3)]
    def hit(name, sig, b, pos, note, vel=1.0, pan=0.0):
        place(P["drums"], sig * vel, at(b, pos), pan)
        notes["drums"].append((b * 4 + pos, 0.2, note, int(max(1, min(127, 127 * vel)))))
    for b in range(bars):
        if b in INTRO or b in BREAK:
            continue
        if b in B:                                             # four on the floor
            for i in range(4):
                hit("k", k, b, i, 36)
                hit("oh", oh, b, i + 0.5, 46, 0.9, -0.2)
            hit("c", cl, b, 1, 39); hit("c", cl, b, 3, 39)
            for i in range(16):
                hit("h", hats[i % 3], b, i / 4, 42, 0.4 + 0.3 * rng.random(), 0.3)
        else:                                                  # half-time rage trap
            for pos in [0, 0.75, 2.5] + ([1.5] if b % 2 else []) + ([3.25, 3.75] if b % 4 == 3 else []):
                hit("k", k, b, pos, 36)
            hit("c", cl, b, 2, 39)
            pos = 0.0
            while pos < 4:
                roll = rng.random() < 0.22
                if roll:                                       # 1/32 or triplet roll
                    count, span = (4, 0.5) if rng.random() < 0.5 else (3, 0.5)
                    for r in range(count):
                        hit("h", hats[r % 3], b, pos + r * span / count, 42, 0.5 + 0.5 * r / count, 0.3)
                    pos += span
                else:
                    hit("h", hats[int(pos * 4) % 3], b, pos, 42, 0.6 + 0.4 * rng.random(), 0.3)
                    pos += 0.25
            if b % 2 == 1:
                hit("oh", oh, b, 3.5, 46, 1.0, -0.2)
    for r in range(32):                                        # snare build through the break
        pos = r * 8 / 32 if r < 16 else 4 + (r - 16) * 4 / 16
        if r >= 16 or r % 2 == 0:
            b0 = 12 + int(pos // 4)
            place(P["drums"], resample(sn, 1 + r / 40) * (0.25 + 0.75 * r / 32), at(b0, pos % 4))
            notes["drums"].append((b0 * 4 + pos % 4, 0.1, 38, int(40 + 80 * r / 32)))

    # --- blown-out 808 + growl fills ---
    for b in drop:
        root = ROOTS[b % 4]
        fill = (b % 4 == 3) and b not in OUTRO
        pat = [(0, 0.7), (0.75, 1.7), (2.5, 1.4)] if b not in B else [(0, 1.0), (1.5, 1.0), (2.5, 0.5), (3, 1.0)]
        if fill:
            pat = [p for p in pat if p[0] < 2.5] + [(2.5, 0.5)]
        prev = ROOTS[(b - 1) % 4]
        for j, (pos, ln) in enumerate(pat):
            m = root + (12 if (j and rng.random() < 0.25) else 0)
            notes["808"].append((b * 4 + pos, ln, m, 125))
            n = int((ln * beat) * SR)
            t = np.arange(n) / SR
            f = (mtof(prev) + (mtof(m) - mtof(prev)) * (1 - np.exp(-t * 35))) * (1 + 1.2 * np.exp(-t * 80))
            sine = np.sin(2 * np.pi * np.cumsum(f) / SR)
            y = (np.clip(sine * 8, -1, 1) * 0.6 + sine * 0.7) * adsr(n, 0.001, 1.5, 0.85, 0.03, n)
            place(P["bass"], y * 0.8, at(b, pos))
            prev = m
        if fill:
            for i, off in enumerate([3.0, 3.25, 3.5, 3.75]):
                place(P["bass"], growl(root + 12 + [0, 3, 0, 7][i], int(step * SR)) * 0.5, at(b, off))

    # --- fx: risers and impacts ---
    for start, end in ((2, 4), (12, 14)):
        n = at(end) - at(start)
        up = rng.standard_normal((2, n)) * np.linspace(0, 1, n) ** 2
        tone = np.sin(2 * np.pi * np.cumsum(np.linspace(200, 3000, n)) / SR) * np.linspace(0, 0.5, n)
        P["fx"][:, at(start):at(end)] += (up - onepole_lp(up, 3000)) * 0.15 + tone * 0.08
    for b in (4, 14, 22):
        n = int(2.0 * SR)
        crash = rng.standard_normal((2, n)) * np.exp(-np.arange(n) / SR * 2.5)
        boom = np.sin(2 * np.pi * np.cumsum(30 + 60 * np.exp(-np.arange(n) / SR * 4)) / SR) * np.exp(-np.arange(n) / SR * 3)
        P["fx"][:, at(b):at(b) + n] += (crash - onepole_lp(crash, 3500)) * 0.2 + boom * 0.5

    # --------------------------------------------------------- mix ---
    kick_env = np.zeros(total)
    for st, _, nt, _ in notes["drums"]:
        if nt == 36:
            s = int(st * beat * SR)
            d = np.exp(-np.arange(int(0.2 * SR)) / SR * 13)
            e = min(total, s + len(d))
            kick_env[s:e] = np.maximum(kick_env[s:e], d[: e - s])
    pump = 1 - 0.8 * kick_env

    stab = P["stab"]
    sweep = np.ones(total)
    sweep[:at(4)] = np.linspace(0.05, 1, at(4)) ** 2
    stab = onepole_lp(stab, 500) * (1 - sweep) + stab * sweep
    br0, br1 = at(12), at(14)
    stab[:, br0:br1] = onepole_lp(stab[:, br0:br1], 1200) * 0.8

    music = (stab + P["lead"] + P["arp"] + P["vox"]) * pump
    wet = reverb(music * 0.6, seconds=2.0)
    bus = music + wet * 0.7 + P["bass"] * pump ** 0.5 + P["drums"] + P["fx"]

    # outro: crushed + decimated
    o0, o1 = at(22), at(24)
    bus[:, o0:o1] = decimate(crush(np.tanh(bus[:, o0:o1] * 1.5), 6), 5)

    # glitch edits at phrase ends
    for b in list(A)[1::2] + list(B)[1::2] + list(OUTRO):
        kind = int(rng.integers(0, 4))
        if kind == 0:
            stutter(bus, at(b, 3), step, 4)
        elif kind == 1:
            stutter(bus, at(b, 3.5), step / 2, 4, pitch_up=True)
        elif kind == 2:
            reverse_chunk(bus, at(b, 3), beat)
        else:
            bus[:, at(b, 3.5):at(b, 3.75)] = 0.0             # hard silence gap
    tape_stop(bus, at(3, 3), beat)
    bus[:, at(4) - int(step * SR):at(4)] = 0.0
    tape_stop(bus, at(11, 3), beat)
    bus[:, at(14) - int(step * SR):at(14)] = 0.0
    tape_stop(bus, at(23, 2), 2 * beat)

    bus = ott(bus)
    bus = onepole_lp(bus, 11000) * 0.7 + bus * 0.3
    bus *= 0.95 / (np.max(np.abs(bus)) + 1e-9)
    return bus, notes
