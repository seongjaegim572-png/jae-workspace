#pragma once

// Halo voice: two layers, each either a wavetable oscillator or a granular engine,
// with its own filter and amp envelope, plus a shared mod env, 2 LFOs and a mod matrix.
#include "DspBits.h"
#include "Wavetables.h"
#include "Textures.h"
#include <array>

namespace halo
{
constexpr int kLayers = 2, kSlots = 4, kMacros = 2;

namespace src { enum { None, Lfo1, Lfo2, ModEnv, Velocity, ModWheel, Random, Macro1, Macro2, Count }; }
namespace dst { enum { None, APos, BPos, ACutoff, BCutoff, ALevel, BLevel, GrainSize, GrainDensity, GrainSpray, Pitch, Pan, Count }; }

inline juce::StringArray sourceNames() { return { "None", "LFO 1", "LFO 2", "Mod Env", "Velocity", "Mod Wheel", "Random", "Macro 1", "Macro 2" }; }
inline juce::StringArray destNames()
{
    return { "None", "A Position", "B Position", "A Cutoff", "B Cutoff", "A Level", "B Level",
             "Grain Size", "Grain Density", "Grain Spray", "Pitch", "Pan" };
}

struct LayerSettings
{
    bool on = true;
    int source = 0;              // 0 = wavetable, 1 = granular
    int table = 0;
    float pos = 0.67f;
    const SampleData* texture = nullptr;
    float root = 60.0f;          // note the texture was recorded at
    float grainPos = 0.3f, spray = 0.2f, grainMs = 150.0f, density = 30.0f, jitter = 0.0f;
    float detune = 0.2f, coarse = 0.0f, fine = 0.0f, level = 0.8f, pan = 0.0f;
    int filterType = 0;          // 0 LP, 1 BP, 2 HP
    float cutoff = 8000.0f, res = 0.1f;
    float a = 0.01f, d = 0.3f, s = 0.8f, r = 0.5f;
};

struct Settings
{
    std::array<LayerSettings, kLayers> layer;
    std::array<int, 2> lfoShape {};
    std::array<float, 2> lfoRate { 0.2f, 0.5f };
    float ma = 0.5f, md = 1.0f, ms = 0.5f, mr = 1.0f;
    std::array<int, kSlots> slotSrc {}, slotDst {};
    std::array<float, kSlots> slotAmt {};
    std::array<float, kMacros> macros {};
    float velSens = 0.5f, modWheel = 0.0f;
};

class Voice
{
public:
    static constexpr int kCtrl = 32;
    static constexpr int kMaxGrains = 24;

    void prepare (double sampleRate, uint32_t seed)
    {
        sr = (float) sampleRate;
        rng.s = seed | 1u;
        for (auto& l : layers) l.env.setSampleRate (sampleRate);
        modEnv.setSampleRate (sampleRate);
        active = held = false;
    }

    bool isActive() const noexcept   { return active; }
    bool isHeld() const noexcept     { return held; }
    bool isReleasing() const noexcept { return active && ! held; }
    int getNote() const noexcept     { return note; }
    uint64_t getAge() const noexcept { return age; }

    void start (int newNote, float velocity, uint64_t order, const Settings& s)
    {
        const bool wasActive = active;
        note = newNote; vel = velocity; age = order;
        held = true; active = true;
        randomValue = rng.nextBipolar();
        for (int i = 0; i < 2; ++i) lfoPhase[(size_t) i] = rng.next01();
        modEnv.noteOn();

        for (int i = 0; i < kLayers; ++i)
        {
            auto& L = layers[(size_t) i];
            if (! wasActive)
            {
                L.fl.reset(); L.fr.reset();
                L.env.reset();
                for (auto& g : L.grains) g.on = false;
                L.spawnAcc = 1.0f; // first grain immediately
                for (auto& ph : L.phase) ph = rng.next01();
            }
            if (s.layer[(size_t) i].on) L.env.noteOn();
        }
    }

    void keyUp()
    {
        held = false;
        modEnv.noteOff();
        for (auto& L : layers) L.env.noteOff();
    }

    void render (float* outL, float* outR, int n, const Settings& s) noexcept
    {
        if (! active) return;
        for (int i = 0; i < kLayers; ++i)
        {
            const auto& ls = s.layer[(size_t) i];
            layers[(size_t) i].env.setParams (ls.a, ls.d, ls.s, ls.r);
        }
        modEnv.setParams (s.ma, s.md, s.ms, s.mr);

        int pos = 0;
        while (pos < n && active)
        {
            const int c = juce::jmin (kCtrl, n - pos);
            update (c, s);
            renderChunk (outL + pos, outR + pos, c);
            pos += c;
        }
    }

private:
    struct Grain
    {
        bool on = false;
        double pos = 0.0, inc = 1.0;
        int age = 0, len = 1;
        float gl = 1.0f, gr = 1.0f;
    };

    struct Layer
    {
        jae::Envelope env;
        jae::Svf fl, fr;
        jae::SvfCoefs coefs;
        int filterMode = 0;
        bool on = false, granular = false;

        // wavetable
        std::array<float, 3> phase {}, inc {};
        const float* t0[3] {};
        const float* t1[3] {};
        float frac = 0.0f;

        // granular
        const SampleData* tex = nullptr;
        std::array<Grain, kMaxGrains> grains {};
        float spawnAcc = 0.0f, spawnInc = 0.0f;
        double baseInc = 1.0;
        float grainPos = 0.0f, spray = 0.0f, jitter = 0.0f, grainGain = 1.0f;
        int grainLen = 1000;

        float gainL = 1.0f, gainR = 1.0f, prevL = 0.0f, prevR = 0.0f;
    };

    static float hann (float x) noexcept
    {
        const float s = std::sin (juce::MathConstants<float>::pi * x);
        return s * s;
    }

    float lfoValue (int i, int shape) const noexcept
    {
        const float p = lfoPhase[(size_t) i];
        switch (shape)
        {
            case 1:  return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
            case 2:  return 1.0f - 2.0f * p;
            case 3:  return p < 0.5f ? 1.0f : -1.0f;
            case 4:  return lfoHold[(size_t) i];
            default: return std::sin (juce::MathConstants<float>::twoPi * p);
        }
    }

    void update (int n, const Settings& s) noexcept
    {
        std::array<float, src::Count> sources {};
        for (int i = 0; i < 2; ++i)
        {
            sources[(size_t) (src::Lfo1 + i)] = lfoValue (i, s.lfoShape[(size_t) i]);
            float& p = lfoPhase[(size_t) i];
            p += s.lfoRate[(size_t) i] * (float) n / sr;
            if (p >= 1.0f) { p -= std::floor (p); lfoHold[(size_t) i] = rng.nextBipolar(); }
        }
        sources[src::ModEnv] = modEnv.getValue();
        sources[src::Velocity] = vel;
        sources[src::ModWheel] = s.modWheel;
        sources[src::Random] = randomValue;
        sources[src::Macro1] = s.macros[0];
        sources[src::Macro2] = s.macros[1];

        std::array<float, dst::Count> m {};
        for (int i = 0; i < kSlots; ++i)
        {
            const int a = s.slotSrc[(size_t) i], b = s.slotDst[(size_t) i];
            if (a > src::None && a < src::Count && b > dst::None && b < dst::Count)
                m[(size_t) b] += s.slotAmt[(size_t) i] * sources[(size_t) a];
        }

        const float velGain = (1.0f - s.velSens) + s.velSens * vel;
        const auto& bank = jae::WavetableBank::get();

        for (int i = 0; i < kLayers; ++i)
        {
            const auto& ls = s.layer[(size_t) i];
            auto& L = layers[(size_t) i];
            L.on = ls.on;
            if (! L.on) continue;

            const float pitch = (float) note + ls.coarse + ls.fine * 0.01f + 12.0f * m[dst::Pitch];
            const float posMod = m[(size_t) (dst::APos + i)];
            L.granular = ls.source == 1 && ls.texture != nullptr && ! ls.texture->data.empty();

            if (L.granular)
            {
                L.tex = ls.texture;
                float gp = ls.grainPos + posMod;
                L.grainPos = gp - std::floor (gp);
                L.spray = juce::jlimit (0.0f, 1.0f, ls.spray + m[dst::GrainSpray]);
                L.jitter = ls.jitter;
                const float ms = juce::jlimit (5.0f, 2000.0f, ls.grainMs * std::exp2 (2.0f * m[dst::GrainSize]));
                const float dens = juce::jlimit (1.0f, 200.0f, ls.density * std::exp2 (2.0f * m[dst::GrainDensity]));
                L.grainLen = juce::jmax (32, (int) (ms * 0.001f * sr));
                L.spawnInc = dens / sr;
                L.grainGain = 1.0f / std::sqrt (juce::jmax (1.0f, dens * ms * 0.001f));
                L.baseInc = L.tex->sampleRate / (double) sr * std::exp2 ((pitch - ls.root) / 12.0);
            }
            else
            {
                const float position = juce::jlimit (0.0f, 1.0f, ls.pos + posMod);
                const float fpos = position * (float) (jae::kNumFrames - 1);
                const int f0 = juce::jmin (jae::kNumFrames - 2, (int) fpos);
                L.frac = fpos - (float) f0;
                const auto& table = bank.table (ls.table);
                const float hz = 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f);
                for (int u = 0; u < 3; ++u)
                {
                    const float cents = (float) (u - 1) * ls.detune * 25.0f;
                    const float inc = juce::jlimit (0.0f, 0.45f, hz * std::exp2 (cents / 1200.0f) / sr);
                    const int lvl = jae::WavetableBank::levelForIncrement (inc);
                    L.inc[(size_t) u] = inc;
                    L.t0[u] = table.get (f0, lvl);
                    L.t1[u] = table.get (f0 + 1, lvl);
                }
            }

            const float fc = juce::jlimit (20.0f, juce::jmin (20000.0f, sr * 0.45f),
                                           ls.cutoff * std::exp2 (5.0f * m[(size_t) (dst::ACutoff + i)]));
            L.coefs.set (std::tan (juce::MathConstants<float>::pi * fc / sr), 2.0f - 1.9f * ls.res);
            L.filterMode = ls.filterType;

            const float level = juce::jlimit (0.0f, 1.0f, ls.level + m[(size_t) (dst::ALevel + i)]) * velGain * 0.3f;
            const float pan = juce::jlimit (-1.0f, 1.0f, ls.pan + m[dst::Pan]);
            L.gainL = level * juce::jmin (1.0f, 1.0f - pan);
            L.gainR = level * juce::jmin (1.0f, 1.0f + pan);
        }
    }

    void spawnGrain (Layer& L) noexcept
    {
        for (auto& g : L.grains)
        {
            if (g.on) continue;
            const double size = (double) L.tex->data.size();
            double start = (double) L.grainPos + (double) (L.spray * rng.nextBipolar() * 0.5f);
            start -= std::floor (start);
            g.on = true;
            g.pos = start * size;
            g.inc = L.baseInc * std::exp2 ((double) (L.jitter * rng.nextBipolar() * 0.5f) / 12.0);
            g.age = 0;
            g.len = L.grainLen;
            const float pan = rng.nextBipolar() * (0.25f + 0.75f * L.spray);
            const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            g.gl = std::cos (a) * 1.414f;
            g.gr = std::sin (a) * 1.414f;
            return;
        }
    }

    void renderChunk (float* outL, float* outR, int n) noexcept
    {
        bool anyActive = false;

        for (auto& L : layers)
        {
            if (! L.env.isActive()) continue;
            anyActive = true;
            const float stepL = (L.gainL - L.prevL) / (float) n, stepR = (L.gainR - L.prevR) / (float) n;

            for (int i = 0; i < n; ++i)
            {
                float l = 0.0f, r = 0.0f;

                if (L.granular)
                {
                    L.spawnAcc += L.spawnInc;
                    if (L.spawnAcc >= 1.0f) { L.spawnAcc -= 1.0f; spawnGrain (L); }

                    const auto& data = L.tex->data;
                    const int size = (int) data.size();
                    for (auto& g : L.grains)
                    {
                        if (! g.on) continue;
                        if (g.pos >= (double) size) g.pos = std::fmod (g.pos, (double) size); // sample was swapped
                        const int i0 = (int) g.pos;
                        const float fr = (float) (g.pos - (double) i0);
                        const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
                        const float smp = data[(size_t) i0] + fr * (data[(size_t) i1] - data[(size_t) i0]);
                        const float w = hann ((float) g.age / (float) g.len) * L.grainGain;
                        l += smp * w * g.gl;
                        r += smp * w * g.gr;
                        g.pos += g.inc;
                        while (g.pos >= (double) size) g.pos -= (double) size;
                        if (++g.age >= g.len) g.on = false;
                    }
                }
                else
                {
                    float y = 0.0f;
                    for (int u = 0; u < 3; ++u)
                    {
                        const float idx = L.phase[(size_t) u] * (float) jae::kTableSize;
                        const int i0 = juce::jlimit (0, jae::kTableSize - 1, (int) idx);
                        const float fr = idx - (float) i0;
                        const float a = L.t0[u][i0] + fr * (L.t0[u][i0 + 1] - L.t0[u][i0]);
                        const float b = L.t1[u][i0] + fr * (L.t1[u][i0 + 1] - L.t1[u][i0]);
                        y += a + L.frac * (b - a);
                        float p = L.phase[(size_t) u] + L.inc[(size_t) u];
                        if (p >= 1.0f) p -= 1.0f;
                        L.phase[(size_t) u] = p;
                    }
                    l = r = y * 0.577f;
                }

                l = L.fl.process (l, L.coefs, L.filterMode);
                r = L.fr.process (r, L.coefs, L.filterMode);
                const float env = L.env.next();
                L.prevL += stepL;
                L.prevR += stepR;
                outL[i] += l * env * L.prevL;
                outR[i] += r * env * L.prevR;
            }
            L.prevL = L.gainL;
            L.prevR = L.gainR;
        }

        for (int i = 0; i < n; ++i) modEnv.next();
        if (! anyActive) active = false;
    }

    float sr = 44100.0f;
    jae::XorShift rng;
    bool active = false, held = false;
    int note = 60;
    float vel = 1.0f, randomValue = 0.0f;
    uint64_t age = 0;
    jae::Envelope modEnv;
    std::array<float, 2> lfoPhase {}, lfoHold {};
    std::array<Layer, kLayers> layers {};
};
} // namespace halo
