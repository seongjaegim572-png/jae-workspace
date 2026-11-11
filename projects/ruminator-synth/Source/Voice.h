#pragma once

#include "Params.h"
#include "Wavetables.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace rum
{
struct OscSettings
{
    bool on = true;
    int table = 0;
    float pos = 0.0f, coarse = 0.0f, fine = 0.0f, level = 0.8f;
    int unison = 1;
    float detune = 0.25f, spread = 0.6f, warp = 0.0f, pan = 0.0f;
};

struct SynthSettings
{
    std::array<OscSettings, kNumOscs> osc;
    float subLevel = 0.0f; int subShape = 0; float noiseLevel = 0.0f;
    int fltType = filt::LP24;
    float cutoff = 12000.0f, res = 0.1f, drive = 0.0f, fltEnv = 0.0f, fltKey = 0.0f;
    float ampA = 0.005f, ampD = 0.4f, ampS = 0.8f, ampR = 0.3f;
    float modA = 0.001f, modD = 0.5f, modS = 0.0f, modR = 0.3f;
    std::array<int, kNumLfos> lfoShape {};
    std::array<float, kNumLfos> lfoRate {};          // Hz (already tempo-synced if needed)
    std::array<int, kNumSlots> slotSrc {}, slotDst {};
    std::array<float, kNumSlots> slotAmt {};
    std::array<float, kNumMacros> macros {};
    float glide = 0.0f; bool mono = false; float velSens = 0.5f;
};

struct ControllerState
{
    float modWheel = 0.0f, aftertouch = 0.0f, bendSemis = 0.0f;
};

//==============================================================================
class Envelope
{
public:
    void setSampleRate (double s) noexcept { sr = (float) s; }

    void setParams (float a, float d, float s, float r) noexcept
    {
        attackInc = 1.0f / juce::jmax (1.0f, a * sr);
        decayCoef = coef (d);
        sustain = s;
        releaseCoef = coef (r);
    }

    void noteOn() noexcept  { stage = Attack; }
    void noteOff() noexcept { if (stage != Idle) stage = Release; }
    void reset() noexcept   { stage = Idle; value = 0.0f; }
    bool isActive() const noexcept { return stage != Idle; }
    float getValue() const noexcept { return value; }

    float next() noexcept
    {
        switch (stage)
        {
            case Attack:
                value += attackInc;
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay:
                value = sustain + (value - sustain) * decayCoef;
                break;
            case Release:
                value *= releaseCoef;
                if (value < 1.0e-4f) { value = 0.0f; stage = Idle; }
                break;
            case Idle:
            default:
                break;
        }
        return value;
    }

private:
    float coef (float t) const noexcept { return std::exp (-4.6f / juce::jmax (1.0f, t * sr)); }

    enum Stage { Idle, Attack, Decay, Release };
    Stage stage = Idle;
    float sr = 44100.0f, value = 0.0f;
    float attackInc = 0.01f, decayCoef = 0.99f, sustain = 1.0f, releaseCoef = 0.99f;
};

//==============================================================================
// Zero-delay-feedback state variable filter (Zavalishin / Simper).
struct SvfCoefs
{
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 2.0f;

    void set (float g, float kk) noexcept
    {
        k = kk;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    inline void process (float v0, const SvfCoefs& c, float& low, float& band) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low = v2;
        band = v1;
    }
};

//==============================================================================
class Voice
{
public:
    static constexpr int kCtrl = 16; // modulation is computed every 16 samples

    void prepare (double sampleRate, uint32_t seed) noexcept
    {
        sr = (float) sampleRate;
        ampEnv.setSampleRate (sampleRate);
        modEnv.setSampleRate (sampleRate);
        rng = seed | 1u;
        hardReset();
    }

    void hardReset() noexcept
    {
        active = held = sustained = false;
        ampEnv.reset();
        modEnv.reset();
        for (auto* f : { &fL1, &fL2, &fR1, &fR2 }) f->reset();
        prevAmp = 0.0f;
    }

    bool isActive() const noexcept   { return active; }
    bool isHeld() const noexcept     { return held; }
    bool isReleasing() const noexcept { return active && ! held && ! sustained; }
    int getNote() const noexcept     { return note; }
    uint64_t getAge() const noexcept { return age; }

    void start (int newNote, float velocity, float fromNote, uint64_t order, const SynthSettings& s) noexcept
    {
        const bool wasActive = active;
        note = newNote;
        vel = velocity;
        targetNote = (float) newNote;
        currentNote = fromNote;
        age = order;
        held = true;
        sustained = false;
        active = true;

        if (! wasActive)
        {
            for (auto* f : { &fL1, &fL2, &fR1, &fR2 }) f->reset();
            ampEnv.reset();
            modEnv.reset();
            prevAmp = 0.0f;
            subPhase = 0.0f;
            for (int o = 0; o < kNumOscs; ++o)
                for (int u = 0; u < kMaxUnison; ++u)
                    phase[(size_t) o][(size_t) u] = s.osc[(size_t) o].unison > 1 ? random01() : 0.0f;
        }

        for (int l = 0; l < kNumLfos; ++l)
        {
            lfoPhase[(size_t) l] = 0.0f;
            lfoHold[(size_t) l] = random01() * 2.0f - 1.0f;
        }
        randomValue = random01() * 2.0f - 1.0f;
        ampEnv.noteOn();
        modEnv.noteOn();
    }

    void glideTo (int newNote) noexcept { note = newNote; targetNote = (float) newNote; }

    void keyUp (bool pedalDown) noexcept
    {
        held = false;
        if (pedalDown) sustained = true;
        else releaseEnvelopes();
    }

    void pedalUp() noexcept
    {
        if (sustained && ! held)
        {
            sustained = false;
            releaseEnvelopes();
        }
    }

    // Adds this voice's output into outL/outR.
    void render (float* outL, float* outR, int numSamples, const SynthSettings& s, const ControllerState& c) noexcept
    {
        if (! active) return;

        ampEnv.setParams (s.ampA, s.ampD, s.ampS, s.ampR);
        modEnv.setParams (s.modA, s.modD, s.modS, s.modR);

        int pos = 0;
        while (pos < numSamples && active)
        {
            const int n = juce::jmin (kCtrl, numSamples - pos);
            updateControl (n, s, c);
            renderChunk (outL + pos, outR + pos, n);
            pos += n;
        }
    }

private:
    struct OscCtrl
    {
        bool on = false, warp = false;
        int uni = 1;
        float frac = 0.0f, knee = 0.5f;
        std::array<const float*, kMaxUnison> t0 {}, t1 {};
        std::array<float, kMaxUnison> inc {}, gl {}, gr {};
    };

    void releaseEnvelopes() noexcept
    {
        ampEnv.noteOff();
        modEnv.noteOff();
    }

    float random01() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng >> 8) * (1.0f / 16777216.0f);
    }

    float lfoValue (int l, int shape) const noexcept
    {
        const float p = lfoPhase[(size_t) l];
        switch (shape)
        {
            case lfoShape::Triangle:   return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
            case lfoShape::Saw:        return 1.0f - 2.0f * p;
            case lfoShape::Square:     return p < 0.5f ? 1.0f : -1.0f;
            case lfoShape::SampleHold: return lfoHold[(size_t) l];
            case lfoShape::Sine:
            default:                   return std::sin (juce::MathConstants<float>::twoPi * p);
        }
    }

    static float noteToHz (float n) noexcept { return 440.0f * std::exp2 ((n - 69.0f) / 12.0f); }

    void updateControl (int n, const SynthSettings& s, const ControllerState& c) noexcept
    {
        // Glide
        if (s.glide > 0.0005f)
            currentNote += (targetNote - currentNote) * (1.0f - std::exp (-(float) n / (s.glide * 0.35f * sr)));
        else
            currentNote = targetNote;

        // LFOs (rate can be modulated by the previous chunk's matrix result)
        std::array<float, kNumLfos> lfo {};
        for (int l = 0; l < kNumLfos; ++l)
        {
            lfo[(size_t) l] = lfoValue (l, s.lfoShape[(size_t) l]);
            const float rateMod = std::exp2 (4.0f * mods[(size_t) (dst::Lfo1Rate + l)]);
            float& p = lfoPhase[(size_t) l];
            p += s.lfoRate[(size_t) l] * rateMod * (float) n / sr;
            if (p >= 1.0f)
            {
                p -= std::floor (p);
                lfoHold[(size_t) l] = random01() * 2.0f - 1.0f;
            }
        }

        // Mod matrix
        std::array<float, src::Count> sources {};
        sources[src::Lfo1] = lfo[0];
        sources[src::Lfo2] = lfo[1];
        sources[src::ModEnv] = modEnv.getValue();
        sources[src::AmpEnv] = ampEnv.getValue();
        sources[src::Velocity] = vel;
        sources[src::ModWheel] = c.modWheel;
        sources[src::Aftertouch] = c.aftertouch;
        sources[src::Keytrack] = ((float) note - 60.0f) / 60.0f;
        sources[src::Random] = randomValue;
        for (int m = 0; m < kNumMacros; ++m) sources[(size_t) (src::Macro1 + m)] = s.macros[(size_t) m];

        mods.fill (0.0f);
        for (int i = 0; i < kNumSlots; ++i)
        {
            const int sIdx = s.slotSrc[(size_t) i], dIdx = s.slotDst[(size_t) i];
            if (sIdx > src::None && sIdx < src::Count && dIdx > dst::None && dIdx < dst::Count)
                mods[(size_t) dIdx] += s.slotAmt[(size_t) i] * sources[(size_t) sIdx];
        }

        const auto& bank = WavetableBank::get();
        const float panMod = mods[dst::Pan];

        for (int o = 0; o < kNumOscs; ++o)
        {
            const auto& os = s.osc[(size_t) o];
            auto& ctl = oc[(size_t) o];
            ctl.on = os.on && os.level + mods[(size_t) (dst::Osc1Level + o)] > 0.0f;
            if (! ctl.on) continue;

            const float position = juce::jlimit (0.0f, 1.0f, os.pos + mods[(size_t) (dst::Osc1Pos + o)]);
            const float pitch = currentNote + os.coarse + os.fine * 0.01f + c.bendSemis
                                + 24.0f * (mods[(size_t) (dst::Osc1Pitch + o)] + mods[dst::AllPitch]);
            const float warp = juce::jlimit (-1.0f, 1.0f, os.warp + mods[(size_t) (dst::Osc1Warp + o)]);
            const float level = juce::jlimit (0.0f, 1.0f, os.level + mods[(size_t) (dst::Osc1Level + o)]);
            const float detune = juce::jlimit (0.0f, 1.0f, os.detune + mods[(size_t) (dst::Osc1Detune + o)]);
            const int uni = juce::jlimit (1, kMaxUnison, os.unison);

            const float fpos = position * (float) (kNumFrames - 1);
            const int f0 = juce::jmin (kNumFrames - 2, (int) fpos);
            ctl.frac = fpos - (float) f0;
            ctl.uni = uni;
            ctl.warp = std::abs (warp) > 0.001f;
            ctl.knee = 0.5f - warp * 0.45f;

            const auto& table = bank.table (os.table);
            const float baseHz = noteToHz (pitch);
            const float gain = level / std::sqrt ((float) uni);

            for (int u = 0; u < uni; ++u)
            {
                const float spreadPos = uni > 1 ? (float) u / (float) (uni - 1) * 2.0f - 1.0f : 0.0f;
                const float cents = spreadPos * detune * 50.0f;
                const float inc = juce::jlimit (0.0f, 0.45f, baseHz * std::exp2 (cents / 1200.0f) / sr);
                const int lvl = WavetableBank::levelForIncrement (inc);
                ctl.inc[(size_t) u] = inc;
                ctl.t0[(size_t) u] = table.get (f0, lvl);
                ctl.t1[(size_t) u] = table.get (f0 + 1, lvl);

                const float pan = juce::jlimit (-1.0f, 1.0f, spreadPos * os.spread + os.pan + panMod);
                const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                ctl.gl[(size_t) u] = std::cos (angle) * gain;
                ctl.gr[(size_t) u] = std::sin (angle) * gain;
            }
        }

        subInc = juce::jlimit (0.0f, 0.45f, noteToHz (currentNote - 12.0f + c.bendSemis) / sr);
        subGain = s.subLevel * 0.7f;
        subShape = s.subShape;
        noiseGain = s.noiseLevel * 0.5f;

        // Filter
        const float semis = s.fltEnv * modEnv.getValue() * 72.0f
                            + s.fltKey * (currentNote - 60.0f)
                            + mods[dst::Cutoff] * 60.0f;
        const float fc = juce::jlimit (20.0f, juce::jmin (20000.0f, sr * 0.45f), s.cutoff * std::exp2 (semis / 12.0f));
        const float g = std::tan (juce::MathConstants<float>::pi * fc / sr);
        const float res = juce::jlimit (0.0f, 1.0f, s.res + mods[dst::Resonance]);
        c1.set (g, 2.0f - 1.96f * res);
        c2.set (g, 1.85f);
        fltType = s.fltType;

        const float drive = juce::jlimit (0.0f, 1.0f, s.drive + mods[dst::Drive]);
        driveGain = 1.0f + drive * 12.0f;
        driveNorm = 1.0f / std::sqrt (driveGain);

        const float velGain = 1.0f - s.velSens + s.velSens * vel;
        ampTarget = juce::jlimit (0.0f, 2.0f, 1.0f + mods[dst::Amp]) * velGain * 0.3f;
    }

    inline float readOsc (const OscCtrl& ctl, int u, float p) const noexcept
    {
        if (ctl.warp) // phase distortion
            p = p < ctl.knee ? p * 0.5f / ctl.knee : 0.5f + (p - ctl.knee) * 0.5f / (1.0f - ctl.knee);

        const float idx = p * (float) kTableSize;
        const int i0 = juce::jlimit (0, kTableSize - 1, (int) idx);
        const float fr = idx - (float) i0;
        const float* t0 = ctl.t0[(size_t) u];
        const float* t1 = ctl.t1[(size_t) u];
        const float a = t0[i0] + fr * (t0[i0 + 1] - t0[i0]);
        const float b = t1[i0] + fr * (t1[i0 + 1] - t1[i0]);
        return a + ctl.frac * (b - a);
    }

    void renderChunk (float* outL, float* outR, int n) noexcept
    {
        const float ampStep = (ampTarget - prevAmp) / (float) n;

        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;

            for (int o = 0; o < kNumOscs; ++o)
            {
                const auto& ctl = oc[(size_t) o];
                if (! ctl.on) continue;
                auto& ph = phase[(size_t) o];
                for (int u = 0; u < ctl.uni; ++u)
                {
                    const float smp = readOsc (ctl, u, ph[(size_t) u]);
                    l += smp * ctl.gl[(size_t) u];
                    r += smp * ctl.gr[(size_t) u];
                    float p = ph[(size_t) u] + ctl.inc[(size_t) u];
                    if (p >= 1.0f) p -= 1.0f;
                    ph[(size_t) u] = p;
                }
            }

            if (subGain > 0.0f)
            {
                const float sub = subShape == 0 ? std::sin (juce::MathConstants<float>::twoPi * subPhase)
                                                : (subPhase < 0.5f ? 0.7f : -0.7f);
                l += sub * subGain;
                r += sub * subGain;
            }
            subPhase += subInc;
            if (subPhase >= 1.0f) subPhase -= 1.0f;

            if (noiseGain > 0.0f)
            {
                l += (random01() * 2.0f - 1.0f) * noiseGain;
                r += (random01() * 2.0f - 1.0f) * noiseGain;
            }

            l = std::tanh (l * driveGain) * driveNorm;
            r = std::tanh (r * driveGain) * driveNorm;

            l = filter (l, fL1, fL2);
            r = filter (r, fR1, fR2);

            const float env = ampEnv.next();
            modEnv.next();
            prevAmp += ampStep;
            outL[i] += l * env * prevAmp;
            outR[i] += r * env * prevAmp;
        }

        prevAmp = ampTarget;
        if (! ampEnv.isActive()) active = false;
    }

    inline float filter (float x, Svf& s1, Svf& s2) noexcept
    {
        float low, band;
        s1.process (x, c1, low, band);
        switch (fltType)
        {
            case filt::LP12: return low;
            case filt::LP24: { float low2, band2; s2.process (low, c2, low2, band2); return low2; }
            case filt::BP:   return band * c1.k;
            case filt::HP:   return x - c1.k * band - low;
            case filt::Notch:
            default:         return x - c1.k * band;
        }
    }


    float sr = 44100.0f;
    bool active = false, held = false, sustained = false;
    int note = 60;
    float vel = 1.0f, currentNote = 60.0f, targetNote = 60.0f;
    uint64_t age = 0;
    uint32_t rng = 1u;

    Envelope ampEnv, modEnv;
    std::array<std::array<float, kMaxUnison>, kNumOscs> phase {};
    float subPhase = 0.0f;
    std::array<float, kNumLfos> lfoPhase {}, lfoHold {};
    float randomValue = 0.0f;
    std::array<float, dst::Count> mods {};

    std::array<OscCtrl, kNumOscs> oc {};
    float subInc = 0.0f, subGain = 0.0f, noiseGain = 0.0f;
    int subShape = 0;
    float driveGain = 1.0f, driveNorm = 1.0f;
    int fltType = filt::LP24;
    SvfCoefs c1, c2;
    Svf fL1, fL2, fR1, fR2;
    float ampTarget = 0.0f, prevAmp = 0.0f;
};
} // namespace rum
