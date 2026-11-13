"""
"wall" style (v5). Design comes from looking at spectrograms of reference previews, which showed:
  * the bass is the star: long, flat, distorted 808 slabs with stacked harmonics, retriggered with sharp cuts
  * above the bass there are no clean synth lines, only a diffuse reverb/noise haze with a wiggly voice-like lead
  * drums are sparse and sit inside the bass rhythm
  * sections change every 6-10 s, with full-band silences and stutter gates between them
Original material only (own chords, melody, patterns, synthesised sounds).

24 bars @ 152 BPM, A minor:
  1-2    intro    haze + voice, no bass
  3-10   slab     sustained 808 slabs, half-time clap; silence cut in bar 6, stutter gate in bar 10
  11-12  float    bass out: staccato mid chord blocks + voice, downlifter
  13-18  rapid    fast swooping kick-808 hits
  19-24  sparse   half-time, bass an octave up, more space; tape stop
"""
import numpy as np

from make_beat import SR, mtof, bandlimited_table, osc, adsr, place, onepole_lp, reverb
from chaos import dirt, resample, stutter, tape_stop, supersaw, snare, hat
from rage import clap, ott

CHORDS = [[57, 60, 64, 67, 71], [53, 57, 60, 64, 67], [50, 57, 60, 65, 69], [52, 56, 59, 62, 65]]  # Am9 Fmaj9 Dm7 E7b9
ROOTS = [33, 29, 26, 28]                       # A1 F1 D1 E1
VOICE_SCALE = [64, 67, 69, 72, 74, 76, 79]     # A minor pentatonic, vocal range
# bass rhythms as (start, length) in 16th notes over 2 bars (32 steps)
SLAB_PATTERNS = [
    [(0, 6), (6, 2), (8, 4), (12, 4), (16, 6), (22, 2), (24, 2), (26, 2), (28, 4)],
    [(0, 8), (8, 3), (11, 5), (16, 4), (20, 4), (24, 6), (30, 2)],
]
RAPID_PATTERN = [0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 26, 28, 30]


def slab(m, n, swoop=False):
    """Distorted 808 with stacked harmonics. Flat pitch (slab) or pitch-dropping hit (swoop)."""
    t = np.arange(n) / SR
    f = mtof(m) * (1 + (1.6 * np.exp(-t * 28) if swoop else 0.2 * np.exp(-t * 90)))
    ph = 2 * np.pi * np.cumsum(f) / SR
    sine = np.sin(ph)
    saw = sum(np.sin(k * ph) / k for k in range(1, 13))
    y = onepole_lp(np.tanh(3.0 * (sine + 0.6 * saw)), 550) * 0.5 + sine * 1.0
    env = np.ones(n)
    a, r = int(0.002 * SR), min(n // 2, int(0.012 * SR))
    env[:a] = np.linspace(0, 1, a)
    env[-r:] = np.linspace(1, 0, r)                       # sharp cut, no tail
    if swoop:
        env *= np.exp(-t * 4.5)
    return y * env


def voice(seq, beat, vowel=(650, 1300)):
    """Voice-like lead: gliding pitch with wobble, shaped by formants."""
    total = int((seq[-1][0] + seq[-1][1]) * beat * SR) + int(0.2 * SR)
    freq, amp = np.zeros(total), np.zeros(total)
    prev = seq[0][2]
    for start, ln, m in seq:
        s, n = int(start * beat * SR), int(ln * beat * SR)
        t = np.arange(n) / SR
        midi = m + (prev - m) * np.exp(-t * 30) + 0.35 * np.sin(2 * np.pi * 5.8 * t) * np.minimum(t * 3, 1)
        midi = midi + 0.5 * np.round(np.sin(2 * np.pi * 11 * t) * np.minimum(t * 2, 1))   # autotune-ish flicker
        freq[s:s + n] = mtof(midi)
        amp[s:s + n] = adsr(n, 0.02, 0.3, 0.8, 0.05, int(n * 0.9))
        prev = m
    freq[freq == 0] = mtof(prev)
    y = osc(freq, bandlimited_table(mtof(84), "saw")) * amp
    spec = np.fft.rfft(y)
    f = np.fft.rfftfreq(len(y), 1 / SR)
    g = 0.1 + np.exp(-((f - vowel[0]) / 220) ** 2) + 0.8 * np.exp(-((f - vowel[1]) / 320) ** 2) \
        + 0.45 * np.exp(-((f - 2900) / 500) ** 2)
    return dirt(np.fft.irfft(spec * g, len(y)) * 2.0, 2.0)


def build_wall(bpm=152, seed=82, bars=24):
    rng = np.random.default_rng(seed)
    beat = 60.0 / bpm
    bar = 4 * beat
    step = beat / 4
    total = int((bars * bar + 1.5) * SR)
    at = lambda b, beats=0.0: int((b * bar + beats * beat) * SR)
    P = {k: np.zeros((2, total)) for k in ("haze", "voice", "blocks", "bass", "drums", "fx")}
    notes = {k: [] for k in ("chords", "melody", "808", "drums")}
    INTRO, SLAB, FLOAT, RAPID, SPARSE = range(0, 2), range(2, 10), range(10, 12), range(12, 18), range(18, 24)
    onsets = []                                             # sidechain triggers (sample positions)

    # --- haze source: sustained chords, later smeared by reverb so no clean lines remain ---
    for b in range(0, bars, 2):
        chord = CHORDS[(b // 2) % 4]
        n = int(2 * bar * SR)
        layer = sum(supersaw(m + 12, n, voices=5, spread_cents=40) for m in chord)
        layer = layer - onepole_lp(layer, 400)
        P["haze"][:, at(b):at(b) + n] += layer[:, : total - at(b)] * 0.1
        notes["chords"] += [(b * 4, 8, m + 12, 80) for m in chord]

    # --- voice-like lead: 2-bar phrases with breaths ---
    def phrase():
        seq, pos = [], float(rng.choice([0, 0.5]))
        deg = int(rng.integers(2, 5))
        while pos < 6.5:
            ln = float(rng.choice([0.5, 0.75, 1.0, 1.5, 2.0]))
            deg = int(np.clip(deg + rng.choice([-2, -1, -1, 0, 1, 1, 2]), 0, len(VOICE_SCALE) - 1))
            seq.append((pos, ln, VOICE_SCALE[deg]))
            pos += ln + (0.5 if rng.random() < 0.25 else 0)
        return seq
    phrases = [phrase(), phrase(), phrase()]
    for i, b in enumerate(range(0, bars, 2)):
        if b in (16,):                                      # leave one hole
            continue
        seq = phrases[[0, 1, 0, 2][i % 4]]
        v = voice(seq, beat, [(650, 1300), (500, 1700), (750, 1150)][i % 3])
        place(P["voice"], v * 0.24, at(b), -0.35)
        place(P["voice"], resample(v, 2.0)[: len(v)] * 0.05, at(b), 0.5)          # faint octave-up double
        place(P["voice"], v * 0.17, at(b) + int(0.021 * SR), 0.45)
        notes["melody"] += [(b * 4 + s, ln, m, 100) for s, ln, m in seq]

    # --- float section: staccato mid chord blocks ---
    for b in FLOAT:
        chord = CHORDS[(b // 2) % 4]
        for i in range(8):
            n = int(beat * 0.5 * SR)
            y = sum(np.sign(np.sin(2 * np.pi * mtof(m) * np.arange(n) / SR)) for m in chord[:4])
            y = onepole_lp(y, 1400) * adsr(n, 0.002, 0.1, 0.3, 0.02, int(n * 0.6))
            place(P["blocks"], y * 0.05, at(b, i * 0.5), 0.3 * (-1) ** i)

    # --- bass ---
    def bass_note(b, s16, len16, m, swoop=False, gain=1.0):
        n = int(len16 * step * SR) - int(0.02 * SR)        # small gap before the next note = the "cut"
        place(P["bass"], slab(m, n, swoop) * gain, at(b) + int(s16 * step * SR))
        notes["808"].append((b * 4 + s16 / 4, len16 / 4 * 0.95, m, 125))
        onsets.append(at(b) + int(s16 * step * SR))
    for b in list(SLAB)[::2]:
        root = ROOTS[(b // 2) % 4]
        for s16, ln in SLAB_PATTERNS[(b // 2) % 2]:
            m = root + (12 if (ln <= 2 and rng.random() < 0.5) else 0)
            bass_note(b, s16, ln, m)
    for b in list(RAPID)[::2]:
        root = ROOTS[(b // 2) % 4]
        pat = RAPID_PATTERN
        for j, s16 in enumerate(pat):
            nxt = pat[j + 1] if j + 1 < len(pat) else 32
            bass_note(b, s16, nxt - s16, root + (7 if j % 5 == 4 else 0), swoop=True, gain=1.1)
    for b in list(SPARSE)[::2]:
        root = ROOTS[(b // 2) % 4] + 12
        for s16, ln in [(0, 14), (16, 6), (22, 8)]:
            bass_note(b, s16, ln, root, gain=0.8)

    # --- drums: sparse, inside the bass rhythm ---
    cl, sn = clap(rng), snare(rng)
    hats = [hat(rng) for _ in range(3)]
    click = np.random.default_rng(4).standard_normal(200) * np.linspace(1, 0, 200) * 0.5
    def hit(sig, b, pos, note, vel=1.0, pan=0.0):
        place(P["drums"], sig * vel, at(b, pos), pan)
        notes["drums"].append((b * 4 + pos, 0.2, note, int(max(1, min(127, 127 * vel)))))
    for o in onsets:                                        # kick click on every bass hit
        place(P["drums"], click, o)
        notes["drums"].append((o / SR / beat, 0.1, 36, 110))
    for b in list(SLAB) + list(RAPID) + list(SPARSE):
        hit(cl, b, 2, 39, 1.0)
        hit(sn, b, 2, 38, 0.6)
        if b in RAPID:
            hit(sn, b, 3.5, 38, 0.5)
        dens = 0.5 if b in SPARSE else 1.0
        pos = 0.0
        while pos < 4:
            if rng.random() < 0.18 * dens:
                for r in range(4):
                    hit(hats[r % 3], b, pos + r / 8, 42, 1.0 + 0.4 * r, -0.4)
                pos += 0.5
            else:
                if rng.random() < dens:
                    hit(hats[int(pos * 2) % 3], b, pos, 42, 1.3 + 0.5 * rng.random(), 0.4)
                pos += 0.5

    # --- fx: downlifter in float, impacts at section starts ---
    n = at(12) - at(11)
    sweep = np.sin(2 * np.pi * np.cumsum(np.geomspace(4000, 300, n)) / SR) * np.linspace(0.4, 0.8, n)
    P["fx"][:, at(11):at(12)] += sweep * 0.1
    for b in (2, 12, 18):
        m = int(1.5 * SR)
        crash = rng.standard_normal((2, m)) * np.exp(-np.arange(m) / SR * 3)
        P["fx"][:, at(b):at(b) + m] += (crash - onepole_lp(crash, 3000)) * 0.15

    # --------------------------------------------------------- mix ---
    duck_env = np.zeros(total)
    d = np.exp(-np.arange(int(0.22 * SR)) / SR * 12)
    for o in onsets:
        e = min(total, o + len(d))
        duck_env[o:e] = np.maximum(duck_env[o:e], d[: e - o])
    pump = 1 - 0.75 * duck_env

    fog = rng.standard_normal((2, total))
    fog = (fog - onepole_lp(fog, 2500)) * 0.07
    smear_in = P["haze"] + P["voice"] * 0.9 + P["blocks"] * 0.6
    haze = reverb(smear_in, seconds=3.0) * 1.6 + P["haze"] * 0.25      # mostly wet: smeared, no clean lines
    haze = np.tanh((haze + fog) * 2.5) * 0.5
    music = (haze + P["voice"] * 0.9 + P["blocks"]) * pump
    bus = music + P["bass"] * 0.95 + P["drums"] * 0.9 + P["fx"]

    # edits: silence cut, stutter gate, section silences, tape stop
    bus[:, at(5, 3):at(6)] = 0.0                                       # 1-beat total silence
    g0 = at(9, 2)
    for i in range(8):                                                 # 16th on/off gate, 2 beats
        if i % 2:
            bus[:, g0 + int(i * step * SR):g0 + int((i + 1) * step * SR)] = 0.0
    stutter(bus, at(15, 3), step, 4)
    stutter(bus, at(17, 2), step / 2, 8, pitch_up=True)
    bus[:, at(17, 3.5):at(18)] = 0.0
    stutter(bus, at(21, 3.5), step / 2, 4)
    tape_stop(bus, at(23, 2), 2 * beat)

    bus = ott(bus * 0.7)
    mid, side = (bus[0] + bus[1]) / 2, (bus[0] - bus[1]) / 2 * 0.6     # references are narrower
    bus = np.stack([mid + side, mid - side])
    bus *= 0.95 / (np.max(np.abs(bus)) + 1e-9)
    return bus, notes
