"""
"pump" style (v6). Design comes from spectrograms of reference previews by several artists in the
electro / rage-pop lane, which showed:
  * kick-driven: a hard kick on a steady grid, and the WHOLE mix ducks around every kick (strong pump)
  * bright, full-spectrum supersaw chords with clean harmonics (not a fog), chopped by the pump
  * a harmonically rich saw bass (60-500 Hz) rather than a pure sub
  * a simple, repeated lead hook
  * short dropouts where one element plays alone, then everything slams back
Original material only (own chords, hook, patterns, synthesised sounds).

24 bars @ 150 BPM, A minor:
  1-2    intro      hook alone, band-passed
  3-10   drop A     3-3-2 kick pattern, saw bass, pumped supersaws, hook; dropout every 4th bar
  11-12  breakdown  chords + hook, no drums, riser, snare build
  13-20  drop B     four-on-the-floor, clap on 2 and 4, hook an octave up + answering arp
  21-24  outro      half-time, huge sustained distorted bass chords, hard stop
"""
import numpy as np

from make_beat import SR, mtof, bandlimited_table, osc, adsr, place, onepole_lp, reverb
from chaos import dirt, resample, stutter, supersaw, snare, hat
from rage import hard_kick, clap, open_hat, ott

CHORDS = [[57, 60, 64, 67], [53, 57, 60, 64], [48, 55, 60, 64], [55, 59, 62, 67]]   # Am7 Fmaj7 C G
ROOTS = [33, 29, 36, 31]                                                         # A1 F1 C2 G1
SCALE = [69, 71, 72, 74, 76, 79, 81, 83, 84]                                     # A minor, lead range
RHYTHMS = [[0, 2, 3, 6, 8, 10, 11, 14], [0, 3, 6, 8, 9, 11, 14], [0, 2, 4, 6, 7, 10, 12, 13]]


def lead_note(m, n, bright=1.0):
    t = np.arange(n) / SR
    f = np.full(n, mtof(m)) * (1 + 0.004 * np.sin(2 * np.pi * 6 * t))
    saw = bandlimited_table(mtof(m), "saw")
    y = osc(f * 1.006, saw) + osc(f * 0.994, saw) + 0.5 * np.sign(np.sin(2 * np.pi * mtof(m) * t))
    return dirt(y * 0.6, 2.5 * bright) * adsr(n, 0.002, 0.12, 0.45, 0.04, int(n * 0.8))


def saw_bass(m, n, chord=False):
    """Harmonically rich bass; chord=True stacks a fifth and octave for the outro."""
    t = np.arange(n) / SR
    out = np.zeros(n)
    for semis in ([0, 7, 12] if chord else [0]):
        table = bandlimited_table(mtof(m + semis), "saw")
        for cents in (-9, 9):
            out += osc(np.full(n, mtof(m + semis) * 2 ** (cents / 1200)), table)
    out += 0.5 * np.sin(2 * np.pi * mtof(m) * t)
    y = onepole_lp(dirt(out * 0.5, 4.0, fold=0.15), 3500)
    env = np.ones(n)
    r = min(n // 2, int(0.015 * SR))
    env[: int(0.003 * SR)] = np.linspace(0, 1, int(0.003 * SR))
    env[-r:] = np.linspace(1, 0, r)
    return y * env


def build_pump(bpm=150, seed=82, bars=24):
    rng = np.random.default_rng(seed)
    beat = 60.0 / bpm
    bar = 4 * beat
    step = beat / 4
    total = int((bars * bar + 1.5) * SR)
    at = lambda b, beats=0.0: int((b * bar + beats * beat) * SR)
    P = {k: np.zeros((2, total)) for k in ("chords", "lead", "bass", "drums", "fx")}
    notes = {k: [] for k in ("chords", "melody", "808", "drums")}
    INTRO, A, BREAK, B, OUTRO = range(0, 2), range(2, 10), range(10, 12), range(12, 20), range(20, 24)
    dropout = lambda b: (b in A or b in B) and b % 4 == 1          # last half-bar goes quiet
    kicks = []

    # --- hook: one bar motif, phrase = A A A' B, the same phrase all song (so it is catchy) ---
    rhythm = RHYTHMS[seed % len(RHYTHMS)]
    deg = 4
    motif = []
    for _ in rhythm:
        deg = int(np.clip(deg + rng.choice([-1, 0, 0, 1, 2, -2]), 1, 7))
        motif.append(deg)
    var = motif[:-3] + [min(8, d + 1) for d in motif[-3:]]
    answer = [max(0, d - 2) for d in motif[:4]] + motif[:len(motif) - 4]
    phrase = [motif, motif, var, answer]
    for b in range(bars):
        if b in OUTRO and b >= 22:
            continue
        degs = phrase[b % 4]
        octave = 12 if b in B else 0
        for i, (s16, d) in enumerate(zip(rhythm, degs)):
            nxt = rhythm[i + 1] if i + 1 < len(rhythm) else 16
            m = SCALE[d] + octave
            n = int((nxt - s16) * step * SR)
            y = lead_note(m, n)
            place(P["lead"], y * 0.3, at(b, s16 / 4), -0.25)
            place(P["lead"], y * 0.16, at(b, s16 / 4) + int(0.75 * beat * SR), 0.5)     # dotted-8th echo
            notes["melody"].append((b * 4 + s16 / 4, (nxt - s16) / 4 * 0.8, m, 110))
        if b in B and b % 2 == 1:                                    # answering arp
            chord = CHORDS[(b // 2) % 4]
            for i in range(8, 16):
                m = chord[i % 4] + 24
                place(P["lead"], lead_note(m, int(step * SR), 0.6) * 0.07, at(b, i / 4), 0.6)

    # --- bright supersaw chords, 2 bars each ---
    for b in range(2, bars, 2):
        chord = CHORDS[(b // 2) % 4]
        n = int(2 * bar * SR)
        layer = sum(supersaw(m + 12, n, voices=7, spread_cents=30) for m in chord)
        layer += supersaw(chord[-1] + 24, n, voices=5, spread_cents=30) * 0.6
        layer = dirt(layer * 0.5, 2.5)
        layer = layer - onepole_lp(layer, 300)
        gain = 0.22 if b in BREAK else 0.4
        P["chords"][:, at(b):at(b) + n] += layer[:, : total - at(b)] * gain
        notes["chords"] += [(b * 4, 8, m + 12, 100) for m in chord]

    # --- drums ---
    k, cl, sn, oh = hard_kick(), clap(rng), snare(rng), open_hat(rng)
    hats = [hat(rng) for _ in range(3)]
    def hit(sig, b, pos, note, vel=1.0, pan=0.0):
        place(P["drums"], sig * vel, at(b, pos), pan)
        notes["drums"].append((b * 4 + pos, 0.2, note, int(max(1, min(127, 127 * vel)))))
    for b in list(A) + list(B) + list(OUTRO):
        if b in A:
            kpos = [0, 0.75, 1.5, 2, 2.75, 3.5]                      # 3-3-2 twice
        elif b in B:
            kpos = [0, 1, 2, 3] + ([3.5] if b % 2 else [])
        else:
            kpos = [0, 2.5] if b < 22 else ([0] if b == 22 else [])
        if dropout(b):
            kpos = [p for p in kpos if p < 2]
        for p in kpos:
            hit(k, b, p, 36, 0.7)
            kicks.append(at(b, p))
        if b in OUTRO:
            if b < 22:
                hit(cl, b, 2, 39)
            continue
        if not dropout(b) or True:
            for p in ([1, 3] if b in B else [2]):
                if not (dropout(b) and p >= 2):
                    hit(cl, b, p, 39)
                    hit(sn, b, p, 38, 0.5)
        for i in range(16):
            if dropout(b) and i >= 8:
                break
            if i % 4 == 2:
                hit(oh, b, i / 4, 46, 1.2, -0.3)
            hit(hats[i % 3], b, i / 4, 42, 1.2 if i % 2 == 0 else 0.7, 0.4 * (1 if i % 2 else -1))
    for r in range(24):                                              # snare build in the breakdown
        pos = r * 0.25 + 2 if r < 8 else 4 + (r - 8) * 0.25
        hit(resample(sn, 1 + r / 30), 10, pos, 38, 0.3 + 0.7 * r / 24)

    # --- saw bass: retriggered on every kick, held until the next one ---
    for b in list(A) + list(B):
        root = ROOTS[(b // 2) % 4]
        if b in A:
            pos = [0, 0.75, 1.5, 2, 2.75, 3.5]
        else:
            pos = [0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5]
        if dropout(b):
            pos = [p for p in pos if p < 2]
        for j, p in enumerate(pos):
            nxt = pos[j + 1] if j + 1 < len(pos) else (2 if dropout(b) else 4)
            m = root + (12 if (b in B and j % 2 == 1) else 0)        # octave bounce in drop B
            place(P["bass"], saw_bass(m, int((nxt - p) * beat * SR)) * 0.36, at(b, p))
            notes["808"].append((b * 4 + p, (nxt - p) * 0.95, m, 120))
    for b in (20, 21, 22):                                           # outro: huge sustained bass chords
        root = ROOTS[(b // 2) % 4]
        place(P["bass"], saw_bass(root + 12, int(bar * SR), chord=True) * 0.6, at(b))
        notes["808"].append((b * 4, 4, root + 12, 127))

    # --- fx ---
    n = at(12) - at(10)
    up = rng.standard_normal((2, n)) * np.linspace(0, 1, n) ** 2
    P["fx"][:, at(10):at(12)] += (up - onepole_lp(up, 3000)) * 0.2
    for b in [2, 12, 20] + [x + 1 for x in range(bars) if dropout(x)]:
        m = int(1.2 * SR)
        crash = rng.standard_normal((2, m)) * np.exp(-np.arange(m) / SR * 4)
        P["fx"][:, at(b):at(b) + m] += (crash - onepole_lp(crash, 4000)) * 0.22

    # --------------------------------------------------------- mix ---
    env = np.zeros(total)
    d = np.exp(-np.arange(int(0.3 * SR)) / SR * 13)
    for s in kicks:
        e = min(total, s + len(d))
        env[s:e] = np.maximum(env[s:e], d[: e - s])
    pump = 1 - 0.85 * env

    lead = P["lead"]
    i0, i1 = at(0), at(2)
    lead[:, i0:i1] = (onepole_lp(lead[:, i0:i1], 2500) - onepole_lp(lead[:, i0:i1], 500)) * 1.5   # "phone" intro
    for b in range(bars):                                             # dropouts: only the lead, thinned
        if dropout(b):
            s0, s1 = at(b, 2), at(b + 1)
            P["chords"][:, s0:s1] = 0.0
            lead[:, s0:s1] = lead[:, s0:s1] - onepole_lp(lead[:, s0:s1], 700)
    music = (P["chords"] + lead * 0.9) * pump
    wet = reverb(P["chords"] * 0.3 + lead * 0.5, seconds=1.6) * pump
    bus = music + wet * 0.6 + P["bass"] * pump + P["drums"] + P["fx"]

    stutter(bus, at(9, 3.5), step / 2, 4, pitch_up=True)
    stutter(bus, at(19, 3), step, 4)
    bus[:, at(11, 3.75):at(12)] = 0.0                                 # breath before drop B
    bus[:, at(23):] *= np.linspace(1, 0, total - at(23))[None, :] ** 3
    bus[:, at(23, 1):] = 0.0                                          # hard stop

    bus = ott(bus * 1.7)
    mid, side = (bus[0] + bus[1]) / 2, (bus[0] - bus[1]) / 2 * 0.55
    bus = np.stack([mid + side, mid - side])
    bus *= 0.95 / (np.max(np.abs(bus)) + 1e-9)
    return bus, notes
