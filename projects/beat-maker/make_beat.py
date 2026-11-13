#!/usr/bin/env python3
"""
Beat maker: writes an original beat as
  - a mixed WAV preview (all sounds synthesised in code, no samples)
  - MIDI files per part (chords, melody, 808, drums) to load into FL Studio

Usage: python3 make_beat.py [output_dir] [--seed N] [--bpm 145]
Only needs numpy.
"""
import argparse
import os
import struct
import wave

import numpy as np

SR = 44100

# ---------------------------------------------------------------- music ---
KEY_ROOT = 54  # F#3
MINOR_PENTA = [0, 3, 5, 7, 10]
# i - VI - III - VII in F# minor (F#m, D, A, E), 2 bars each
CHORDS = [
    [54, 57, 61, 64],   # F#m(add b7 colour: E4)
    [50, 54, 57, 61],   # Dmaj7
    [49, 52, 57, 61],   # A/C#
    [52, 56, 59, 62],   # E(add b7 D)
]
BASS_ROOTS = [30, 26, 33, 28]  # F#1, D1, A1, E1


def mtof(m):
    return 440.0 * 2.0 ** ((np.asarray(m, dtype=float) - 69.0) / 12.0)


# ------------------------------------------------------------ synthesis ---
def bandlimited_table(freq, shape, size=2048):
    """One cycle of a saw/square/bell wave with no harmonics above Nyquist."""
    max_h = max(1, int((SR / 2) / max(freq, 1.0)))
    t = np.arange(size) / size
    y = np.zeros(size)
    if shape == "saw":
        for k in range(1, min(max_h, 60) + 1):
            y += np.sin(2 * np.pi * k * t) / k
    elif shape == "bell":
        for k, a in [(1, 1.0), (2, 0.5), (3, 0.25), (4, 0.3), (6, 0.12)]:
            if k <= max_h:
                y += a * np.sin(2 * np.pi * k * t)
    else:
        y = np.sin(2 * np.pi * t)
    return y / (np.max(np.abs(y)) + 1e-9)


def osc(freqs, table):
    """Wavetable oscillator; freqs is per-sample frequency (array)."""
    phase = np.cumsum(freqs / SR) % 1.0
    idx = phase * len(table)
    i0 = idx.astype(int)
    i1 = (i0 + 1) % len(table)
    fr = idx - i0
    return table[i0] * (1 - fr) + table[i1] * fr


def adsr(n, a, d, s, r, gate):
    """Envelope of length n samples; gate = held samples."""
    env = np.zeros(n)
    a_n, d_n, r_n = max(1, int(a * SR)), max(1, int(d * SR)), max(1, int(r * SR))
    gate = min(gate, n)
    seg = np.arange(gate)
    att = np.minimum(seg / a_n, 1.0)
    dec = s + (1 - s) * np.exp(-np.maximum(seg - a_n, 0) / d_n * 4.6)
    env[:gate] = np.where(seg < a_n, att, dec)
    level = env[gate - 1] if gate > 0 else 0.0
    rel_len = n - gate
    if rel_len > 0:
        env[gate:] = level * np.exp(-np.arange(rel_len) / r_n * 4.6)
    return env


def place(buf, sig, start, pan=0.0):
    """Add a mono signal into a stereo buffer at start (samples) with equal-power pan."""
    start = int(start)
    if start >= buf.shape[1]:
        return
    end = min(buf.shape[1], start + len(sig))
    sig = sig[: end - start]
    ang = (pan + 1) * np.pi / 4
    buf[0, start:end] += sig * np.cos(ang)
    buf[1, start:end] += sig * np.sin(ang)


def onepole_lp(x, cutoff):
    """Cheap low-pass using an FFT brick-ish curve (fast in numpy)."""
    spec = np.fft.rfft(x, axis=-1)
    f = np.fft.rfftfreq(x.shape[-1], 1 / SR)
    spec *= 1.0 / np.sqrt(1 + (f / cutoff) ** 4)
    return np.fft.irfft(spec, n=x.shape[-1], axis=-1)


def reverb(x, seconds=1.8, seed=3):
    """Convolution reverb with a decaying stereo noise impulse."""
    rng = np.random.default_rng(seed)
    n = int(seconds * SR)
    decay = np.exp(-np.arange(n) / n * 6.0)
    ir = rng.standard_normal((2, n)) * decay
    ir = onepole_lp(ir, 6000) * 0.02
    size = x.shape[1] + n
    nfft = 1 << (size - 1).bit_length()
    out = np.fft.irfft(np.fft.rfft(x, nfft) * np.fft.rfft(ir, nfft), nfft)[:, : x.shape[1]]
    return out


# ---------------------------------------------------------------- drums ---
def kick():
    n = int(0.28 * SR)
    t = np.arange(n) / SR
    f = 45 + 110 * np.exp(-t * 35)
    y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 12)
    y[:60] += np.linspace(0.6, 0, 60)  # click
    return np.tanh(y * 1.6)


def clap(rng):
    n = int(0.25 * SR)
    t = np.arange(n) / SR
    noise = rng.standard_normal(n)
    env = np.zeros(n)
    for off in (0.0, 0.008, 0.017):          # three quick hand hits
        env += (t >= off) * np.exp(-np.maximum(t - off, 0) * 70)
    env += 0.4 * np.exp(-t * 14)
    y = noise * env
    y = y - onepole_lp(y, 900)               # high-pass-ish
    return 0.5 * y / np.max(np.abs(y))


def hat(rng, open_=False):
    n = int((0.22 if open_ else 0.05) * SR)
    t = np.arange(n) / SR
    y = rng.standard_normal(n) * np.exp(-t * (12 if open_ else 90))
    y = y - onepole_lp(y, 7000)
    return 0.35 * y / np.max(np.abs(y))


# ------------------------------------------------------------- the beat ---
def build(bpm=145, seed=82, bars=16):
    rng = np.random.default_rng(seed)
    beat = 60.0 / bpm
    bar = 4 * beat
    total = int((bars * bar + 2.5) * SR)
    mix = {k: np.zeros((2, total)) for k in ("pad", "lead", "bass", "drums")}
    notes = {k: [] for k in ("chords", "melody", "808", "drums")}  # (start_beat, len_beat, note, vel)
    intro_bars = 4

    # Chords: detuned saw pad, 2 bars per chord
    for b in range(0, bars, 2):
        chord = CHORDS[(b // 2) % 4]
        start, length = b * bar, 2 * bar
        n = int((length + 1.2) * SR)
        env = adsr(n, 0.25, 1.0, 0.75, 1.0, int(length * SR))
        for m in chord:
            notes["chords"].append((b * 4, 8, m, 80))
            table = bandlimited_table(mtof(m), "saw")
            for d, pan in ((-12, -0.7), (0, 0.0), (12, 0.7)):          # cents, pan
                f = np.full(n, mtof(m) * 2 ** (d / 1200))
                place(mix["pad"], osc(f, table) * env * 0.05, start * SR, pan)

    # Melody: original 2-bar motif in F# minor pentatonic, varied every 4 bars
    scale = [KEY_ROOT + 12 + o + 12 * octv for octv in (0, 1) for o in MINOR_PENTA]
    rhythm = [0, 0.75, 1.5, 2, 3, 3.5, 4.5, 5, 5.75, 6.5]           # beats inside 2 bars
    motif = list(rng.choice(len(scale) - 3, size=len(rhythm)) + 1)
    for b in range(0, bars, 2):
        var = list(motif)
        if (b // 2) % 2 == 1:                                         # answer phrase
            var[-3:] = [min(len(scale) - 1, v + 2) for v in var[-3:]]
        for i, (pos, deg) in enumerate(zip(rhythm, var)):
            nxt = rhythm[i + 1] if i + 1 < len(rhythm) else 8
            dur = min(nxt - pos, 1.0) * 0.9
            m = scale[deg]
            vel = int(70 + 30 * rng.random())
            notes["melody"].append((b * 4 + pos, dur, m, vel))
            n = int((dur * beat + 0.8) * SR)
            f = np.full(n, mtof(m))
            y = osc(f, bandlimited_table(mtof(m), "bell")) * adsr(n, 0.003, 0.35, 0.15, 0.4, int(dur * beat * SR))
            place(mix["lead"], y * 0.11 * vel / 100, (b * bar + pos * beat) * SR, 0.15)

    # Drums + 808 only after the intro
    clap_s, hats = clap(rng), [hat(rng) for _ in range(4)]
    open_hat, kick_s = hat(rng, True), kick()
    kick_pattern = [0, 1.75, 2.5]                     # half-time trap: clap on beat 3
    bass_pattern = [(0, 1.5), (1.75, 0.6), (2.5, 1.4)]
    duck = np.ones(total)
    for b in range(intro_bars, bars):
        t0 = b * bar
        kp = kick_pattern + ([3.5] if b % 2 == 1 else [])
        for k in kp:
            place(mix["drums"], kick_s * 0.9, (t0 + k * beat) * SR)
            notes["drums"].append((b * 4 + k, 0.25, 36, 110))
            s = int((t0 + k * beat) * SR)
            d = 1 - 0.55 * np.exp(-np.arange(int(0.25 * SR)) / SR * 14)       # sidechain
            duck[s : s + len(d)] = np.minimum(duck[s : s + len(d)], d[: max(0, min(len(d), total - s))])
        place(mix["drums"], clap_s, (t0 + 2 * beat) * SR, 0.05)
        notes["drums"].append((b * 4 + 2, 0.25, 39, 110))

        # hats: 1/8 grid, 1/16 and triplet rolls at the end of every 2nd bar
        steps = [i * 0.5 for i in range(8)]
        if b % 2 == 1:
            steps = [i * 0.5 for i in range(6)] + [3 + i / 6 for i in range(6)]
        elif b % 4 == 2:
            steps = [i * 0.5 for i in range(5)] + [2.5 + i * 0.25 for i in range(6)]
        for i, st in enumerate(steps):
            vel = 0.55 + 0.45 * ((i % 2) == 0) * rng.random()
            place(mix["drums"], hats[i % 4] * vel, (t0 + st * beat) * SR, 0.3 * (1 if i % 2 else -1))
            notes["drums"].append((b * 4 + st, 0.1, 42, int(60 + 50 * vel)))
        if b % 4 == 3:
            place(mix["drums"], open_hat * 0.7, (t0 + 3.5 * beat) * SR, -0.2)
            notes["drums"].append((b * 4 + 3.5, 0.4, 46, 90))

        # 808 with slides between notes
        root = BASS_ROOTS[(b // 2) % 4]
        for j, (pos, ln) in enumerate(bass_pattern):
            m = root + (12 if (j == 2 and b % 4 == 3) else 0)          # octave jump
            notes["808"].append((b * 4 + pos, ln, m, 110))
            n = int((ln * beat + 0.15) * SR)
            t = np.arange(n) / SR
            prev = BASS_ROOTS[(b // 2) % 4] if j else BASS_ROOTS[((b - 1) // 2) % 4]
            glide = mtof(prev) + (mtof(m) - mtof(prev)) * (1 - np.exp(-t * 40))
            f = glide * (1 + 0.6 * np.exp(-t * 60))                    # punchy attack drop
            y = np.sin(2 * np.pi * np.cumsum(f) / SR)
            y = np.tanh(2.2 * y) * adsr(n, 0.002, 0.8, 0.7, 0.08, int(ln * beat * SR))
            place(mix["bass"], y * 0.42, (t0 + pos * beat) * SR)

    # ---------------------------------------------------------- mix ---
    intro_end = int(intro_bars * bar * SR)
    sweep = np.ones(total)
    sweep[:intro_end] = np.linspace(0.0, 1.0, intro_end) ** 2
    pad_lp = onepole_lp(mix["pad"], 900)
    pad = pad_lp * (1 - sweep) + mix["pad"] * sweep
    pad *= duck
    lead = mix["lead"].copy()
    dl = int(0.75 * beat * SR)                                         # dotted-8th echo
    lead[0, dl:] += 0.35 * mix["lead"][1, :-dl]
    lead[1, dl:] += 0.35 * mix["lead"][0, :-dl]
    wet = reverb(pad * 0.8 + lead * 1.0)
    out = pad + lead + mix["bass"] + mix["drums"] + wet * 0.9
    out = np.tanh(out * 1.3) / np.tanh(1.3)                            # soft clip
    out *= 0.89 / (np.max(np.abs(out)) + 1e-9)
    return out, notes


# ----------------------------------------------------------------- files ---
def write_wav(path, stereo):
    data = (np.clip(stereo.T, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())


def write_midi(path, notes, bpm, channel=0, name="part"):
    tpb = 480

    def vlq(v):
        out = [v & 0x7F]
        v >>= 7
        while v:
            out.insert(0, (v & 0x7F) | 0x80)
            v >>= 7
        return bytes(out)

    events = []
    for start, length, note, vel in notes:
        on = int(round(start * tpb))
        off = on + max(1, int(round(length * tpb)))
        events.append((on, 1, bytes([0x90 | channel, note, max(1, min(127, vel))])))
        events.append((off, 0, bytes([0x80 | channel, note, 0])))
    events.sort(key=lambda e: (e[0], e[1]))

    track = bytearray()
    track += vlq(0) + b"\xff\x03" + vlq(len(name)) + name.encode()
    track += vlq(0) + b"\xff\x51\x03" + struct.pack(">I", int(60_000_000 / bpm))[1:]
    last = 0
    for t, _, msg in events:
        track += vlq(t - last) + msg
        last = t
    track += vlq(0) + b"\xff\x2f\x00"
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, tpb))
        f.write(b"MTrk" + struct.pack(">I", len(track)) + track)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", nargs="?", default="output")
    ap.add_argument("--seed", type=int, default=82)
    ap.add_argument("--bpm", type=float, default=None)
    ap.add_argument("--style", choices=["trap", "chaos", "rage", "wall"], default="trap")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    if args.style == "chaos":
        from chaos import build_chaos
        args.bpm = args.bpm or 160
        audio, notes = build_chaos(args.bpm, args.seed)
        write_midi(os.path.join(args.out, "vox_chops.mid"), notes["vox"], args.bpm, 3, "Vox")
    elif args.style == "wall":
        from wall import build_wall
        args.bpm = args.bpm or 152
        audio, notes = build_wall(args.bpm, args.seed)
    elif args.style == "rage":
        from rage import build_rage
        args.bpm = args.bpm or 152
        audio, notes = build_rage(args.bpm, args.seed)
        write_midi(os.path.join(args.out, "vox_chops.mid"), notes["vox"], args.bpm, 3, "Vox")
        write_midi(os.path.join(args.out, "arp.mid"), notes["arp"], args.bpm, 4, "Arp")
    else:
        args.bpm = args.bpm or 145
        audio, notes = build(args.bpm, args.seed)
    write_wav(os.path.join(args.out, "beat_preview.wav"), audio)
    write_midi(os.path.join(args.out, "chords.mid"), notes["chords"], args.bpm, 0, "Chords")
    write_midi(os.path.join(args.out, "melody.mid"), notes["melody"], args.bpm, 1, "Melody")
    write_midi(os.path.join(args.out, "808.mid"), notes["808"], args.bpm, 2, "808")
    write_midi(os.path.join(args.out, "drums.mid"), notes["drums"], args.bpm, 9, "Drums")
    peak = np.max(np.abs(audio))
    rms = np.sqrt(np.mean(audio ** 2))
    print(f"length {audio.shape[1] / SR:.1f}s  peak {peak:.2f}  rms {20 * np.log10(rms):.1f} dBFS")
    print("notes:", {k: len(v) for k, v in notes.items()})


if __name__ == "__main__":
    main()
