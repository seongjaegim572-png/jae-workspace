#pragma once

// 82 Keys sound engine: all instruments are synthesised (no samples).
//  - Grand, Vibes, Music Box, Celesta: modal / additive synthesis (decaying sine partials)
//  - Tine EP, Reed EP: FM synthesis + pickup "bark" nonlinearity
//  - Clav: Karplus-Strong plucked string + pickup comb
#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace keys
{
namespace model
{
    enum { Grand, TineEP, ReedEP, Clav, Vibes, MusicBox, Celesta, Count };
}

inline juce::StringArray modelNames()
{
    return { "Grand Piano", "Tine EP", "Reed EP", "Clav", "Vibes", "Music Box", "Celesta" };
}

struct Settings
{
    int model = model::Grand;
    float tone = 0.5f, hardness = 0.5f, decay = 1.0f, release = 0.25f;
    float noise = 0.3f, wear = 0.0f, drive = 0.2f, width = 0.6f, velSens = 0.7f;
};

class Voice
{
public:
    static constexpr int kMaxPartials = 40;

    void prepare (double sampleRate, uint32_t seed)
    {
        sr = (float) sampleRate;
        rng = seed | 1u;
        ks.assign ((size_t) (sampleRate / 20.0) + 16, 0.0f);
        active = held = sustained = false;
    }

    bool isActive() const noexcept   { return active; }
    bool isHeld() const noexcept     { return held; }
    bool isReleasing() const noexcept { return active && ! held && ! sustained; }
    int getNote() const noexcept     { return note; }
    uint64_t getAge() const noexcept { return age; }

    void start (int newNote, float velocity, uint64_t order, const Settings& s)
    {
        note = newNote;
        vel = juce::jlimit (0.0f, 1.0f, velocity);
        age = order;
        held = true;
        sustained = false;
        damped = false;
        active = true;
        outGain = 1.0f;
        mdl = juce::jlimit (0, model::Count - 1, s.model);
        checkCounter = 0;
        dcX = dcY = 0.0f;
        peak = 1.0f;

        const float detuneCents = (random01() * 2.0f - 1.0f) * s.wear * 18.0f;
        const float f = 440.0f * std::exp2 (((float) note - 69.0f) / 12.0f + detuneCents / 1200.0f);
        bright = juce::jlimit (0.0f, 1.0f, s.tone * 0.6f + s.hardness * vel * 0.8f - 0.15f);
        velGain = (1.0f - s.velSens) + s.velSens * std::pow (vel, 1.4f);

        const float pan = juce::jlimit (-1.0f, 1.0f, ((float) note - 64.0f) / 44.0f * s.width);
        panGains (pan, panL, panR);

        numPartials = 0;
        switch (mdl)
        {
            case model::Grand:    setupGrand (f, s); break;
            case model::TineEP:   setupFm (f, s, false); break;
            case model::ReedEP:   setupFm (f, s, true); break;
            case model::Clav:     setupClav (f, s); break;
            case model::Vibes:    setupMallet (f, s, { 1.0f, 3.932f, 9.538f }, { 1.0f, 0.05f + 0.3f * bright, 0.12f * bright },
                                               { 1.0f, 0.35f, 0.15f }, 5.0f * std::exp2 (-((float) note - 60.0f) / 36.0f)); break;
            case model::MusicBox: setupMallet (f, s, { 1.0f, 6.27f, 17.55f }, { 1.0f, 0.25f + 0.3f * bright, 0.1f * bright },
                                               { 1.0f, 0.3f, 0.1f }, 3.0f * std::exp2 (-((float) note - 72.0f) / 30.0f)); break;
            case model::Celesta:
            default:              setupMallet (f, s, { 1.0f, 3.95f, 10.1f }, { 1.0f, 0.15f + 0.2f * bright, 0.05f },
                                               { 1.0f, 0.25f, 0.1f }, 2.2f * std::exp2 (-((float) note - 72.0f) / 30.0f)); break;
        }
    }

    void keyUp (bool pedalDown, const Settings& s)
    {
        held = false;
        if (pedalDown) sustained = true;
        else damp (s);
    }

    void pedalUp (const Settings& s)
    {
        if (sustained && ! held)
        {
            sustained = false;
            damp (s);
        }
    }

    void kill() { active = false; }

    void render (float* L, float* R, int n, const Settings& s) noexcept
    {
        if (! active) return;
        const float drive = s.drive;

        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;

            if (mdl == model::TineEP || mdl == model::ReedEP)
            {
                const float y = renderFm (drive);
                l = y * panL; r = y * panR;
            }
            else if (mdl == model::Clav)
            {
                const float y = renderClav();
                l = y * panL; r = y * panR;
            }
            else
            {
                for (int k = 0; k < numPartials; ++k)
                {
                    const float nc = pc[(size_t) k] * pcw[(size_t) k] - ps[(size_t) k] * psw[(size_t) k];
                    const float ns = pc[(size_t) k] * psw[(size_t) k] + ps[(size_t) k] * pcw[(size_t) k];
                    pc[(size_t) k] = nc;
                    ps[(size_t) k] = ns;
                    l += ns * pgl[(size_t) k];
                    r += ns * pgr[(size_t) k];
                }
            }

            // mechanical noise (hammer, mallet, key thump)
            if (noiseEnv > 1.0e-5f)
            {
                noiseLp += noiseCoef * ((random01() * 2.0f - 1.0f) - noiseLp);
                const float nz = noiseLp * noiseEnv;
                noiseEnv *= noiseDecay;
                l += nz * panL;
                r += nz * panR;
            }

            if (damped) outGain *= dampCoef;
            const float g = outGain * velGain * 0.35f;
            L[i] += l * g;
            R[i] += r * g;
        }

        checkCounter += n;
        if (checkCounter >= 256)
        {
            checkCounter = 0;
            if (currentLevel() * outGain < 2.0e-5f && noiseEnv < 1.0e-4f) active = false;
        }
    }

private:
    static void panGains (float pan, float& l, float& r) noexcept
    {
        const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        l = std::cos (a) * 1.414f;
        r = std::sin (a) * 1.414f;
    }

    float random01() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng >> 8) * (1.0f / 16777216.0f);
    }

    float t60Coef (float seconds) const noexcept { return std::exp (-6.9f / juce::jmax (1.0f, seconds * sr)); }

    void startNoise (float amount, float cutoffHz, float decaySeconds)
    {
        noiseEnv = amount;
        noiseCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / sr);
        noiseDecay = std::exp (-1.0f / juce::jmax (1.0f, decaySeconds * sr));
        noiseLp = 0.0f;
    }

    void damp (const Settings& s)
    {
        damped = true;
        dampCoef = std::exp (-4.6f / juce::jmax (1.0f, s.release * sr));
        if (mdl == model::Clav)
            ksGain = std::pow (10.0f, -3.0f / (0.06f * ksFreq)); // felt mutes the string
        if (mdl != model::Grand && mdl != model::Celesta)
            startNoise (s.noise * 0.06f, 500.0f, 0.004f);     // key-off thump
    }

    void addPartial (float freq, float amp, float seconds, float gl, float gr)
    {
        if (numPartials >= kMaxPartials || freq >= sr * 0.45f || freq <= 0.0f) return;
        const float w = juce::MathConstants<float>::twoPi * freq / sr;
        const float rad = t60Coef (seconds);
        const auto k = (size_t) numPartials++;
        pc[k] = amp; ps[k] = 0.0f;
        pcw[k] = rad * std::cos (w);
        psw[k] = rad * std::sin (w);
        pgl[k] = gl; pgr[k] = gr;
    }

    void setupGrand (float f, const Settings& s)
    {
        const float B = 0.00004f * std::exp2 (((float) note - 21.0f) / 15.0f);    // string stiffness
        const float tBase = s.decay * 14.0f * std::exp2 (-((float) note - 21.0f) / 20.0f);
        const int strings = note < 33 ? 1 : 2;
        const float maxF = juce::jmin (16000.0f, sr * 0.45f);

        // amplitudes first, so we can normalise
        std::array<float, 20> amps {};
        int count = 0;
        float sum = 0.0f;
        for (int h = 1; h <= 20; ++h)
        {
            const float fh = (float) h * f * std::sqrt (1.0f + B * (float) (h * h));
            if (fh > maxF) break;
            const float hammerComb = std::abs (std::sin (juce::MathConstants<float>::pi * (float) h * 0.117f));
            amps[(size_t) (h - 1)] = (0.25f + hammerComb) * std::pow ((float) h, -0.6f)
                                     * std::exp (-((float) h - 1.0f) / (1.0f + 14.0f * bright));
            sum += amps[(size_t) (h - 1)];
            count = h;
        }
        if (count == 0) return;

        for (int st = 0; st < strings; ++st)
        {
            const float cents = st == 0 ? 0.0f : 0.5f + random01() * 0.7f + s.wear * 4.0f;
            const float ratio = std::exp2 (cents / 1200.0f);
            float gl, gr;
            const float strPan = juce::jlimit (-1.0f, 1.0f, ((float) note - 64.0f) / 44.0f * s.width
                                               + (st == 0 ? -0.12f : 0.12f) * s.width);
            panGains (strPan, gl, gr);
            for (int h = 1; h <= count; ++h)
            {
                const float fh = (float) h * f * std::sqrt (1.0f + B * (float) (h * h)) * ratio;
                const float t = tBase / (1.0f + 0.22f * ((float) h - 1.0f));
                addPartial (fh, amps[(size_t) (h - 1)] / sum / (float) strings * 1.2f, t, gl, gr);
            }
        }

        startNoise (s.noise * (0.15f + 0.35f * vel), 1500.0f + 4000.0f * bright, 0.003f);
    }

    void setupMallet (float f, const Settings& s, std::array<float, 3> ratios, std::array<float, 3> amps,
                      std::array<float, 3> tMul, float tBase)
    {
        float sum = 0.0f;
        for (auto a : amps) sum += a;
        for (int k = 0; k < 3; ++k)
            addPartial (f * ratios[(size_t) k], amps[(size_t) k] / sum, s.decay * tBase * tMul[(size_t) k], panL, panR);
        // a quieter, slightly detuned copy of the fundamental for warmth
        addPartial (f * std::exp2 ((1.0f + s.wear * 6.0f) / 1200.0f), 0.15f, s.decay * tBase * 0.8f, panR, panL);
        startNoise (s.noise * 0.25f * vel, 3000.0f + 4000.0f * bright, 0.0015f);
    }

    void setupFm (float f, const Settings& s, bool reed)
    {
        carPhase = modPhase = pingPhase = 0.0f;
        carInc = f / sr;
        modInc = f / sr;
        const float pingF = f * 14.0f;
        pingInc = pingF < sr * 0.45f ? pingF / sr : 0.0f;
        isReed = reed;

        indexFloor = reed ? 0.5f + 0.4f * s.tone : 0.12f + 0.25f * s.tone;
        index = indexFloor + (reed ? 1.2f + 3.0f * bright : 0.6f + 2.4f * bright) * (0.4f + 0.6f * vel);
        indexDecay = t60Coef (reed ? 0.5f : 0.35f);
        pingAmp = reed ? 0.0f : 0.25f * bright * vel;
        pingDecay = t60Coef (0.03f);

        const float t = reed ? s.decay * 3.0f * std::exp2 (-((float) note - 48.0f) / 20.0f)
                             : s.decay * 4.5f * std::exp2 (-((float) note - 40.0f) / 18.0f);
        fmAmp = 0.5f;
        fmDecay = t60Coef (t);
        bark = s.drive * (reed ? 0.4f : 0.6f) + 0.25f * vel * bright;
        startNoise (s.noise * 0.08f * vel, 1200.0f, 0.004f);
    }

    float renderFm (float drive) noexcept
    {
        constexpr float tp = juce::MathConstants<float>::twoPi;
        const float mod = std::sin (tp * modPhase) * index;
        float y = std::sin (tp * carPhase + mod);
        if (pingInc > 0.0f && pingAmp > 1.0e-5f)
        {
            y += std::sin (tp * pingPhase) * pingAmp;
            pingPhase += pingInc; if (pingPhase >= 1.0f) pingPhase -= 1.0f;
            pingAmp *= pingDecay;
        }
        carPhase += carInc; if (carPhase >= 1.0f) carPhase -= 1.0f;
        modPhase += modInc; if (modPhase >= 1.0f) modPhase -= 1.0f;
        index = indexFloor + (index - indexFloor) * indexDecay;

        y *= fmAmp;
        fmAmp *= fmDecay;

        y += bark * y * y * 2.0f;                                  // asymmetric pickup bark
        if (isReed) y = std::tanh (y * (1.5f + drive * 2.0f)) * 0.6f;

        // DC blocker (the bark adds DC)
        const float out = y - dcX + 0.995f * dcY;
        dcX = y; dcY = out;
        return out;
    }

    void setupClav (float f, const Settings& s)
    {
        ksFreq = f;
        ksDelay = juce::jlimit (2.0f, (float) ks.size() - 4.0f, sr / f - 0.5f);
        const float t = s.decay * 1.4f * std::exp2 (-((float) note - 48.0f) / 24.0f);
        ksGain = std::pow (10.0f, -3.0f / (t * f));
        pickDelay = juce::jmax (1.0f, (sr / f) * 0.12f);

        std::fill (ks.begin(), ks.end(), 0.0f);
        ksW = 0;
        // excitation: low-passed noise burst, one period long
        const int len = juce::jmin ((int) ks.size() - 2, (int) std::ceil (sr / f) + 2);
        const float a = 0.15f + 0.8f * bright;
        float lp = 0.0f, mean = 0.0f;
        for (int i = 0; i < len; ++i)
        {
            lp += a * ((random01() * 2.0f - 1.0f) - lp);
            ks[(size_t) i] = lp;
            mean += lp;
        }
        mean /= (float) len;
        for (int i = 0; i < len; ++i) ks[(size_t) i] = (ks[(size_t) i] - mean) * 1.6f;
        ksW = len;
        startNoise (s.noise * 0.1f * vel, 2500.0f, 0.002f);
    }

    float ksRead (float d) const noexcept
    {
        const int size = (int) ks.size();
        float rp = (float) ksW - d;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp % size;
        const int i1 = (i0 + 1) % size;
        const float fr = rp - std::floor (rp);
        return ks[(size_t) i0] + fr * (ks[(size_t) i1] - ks[(size_t) i0]);
    }

    float renderClav() noexcept
    {
        const float y = ksGain * 0.5f * (ksRead (ksDelay) + ksRead (ksDelay + 1.0f));
        const float out = y - ksRead (pickDelay);   // pickup position comb -> twang
        ks[(size_t) ksW] = y;
        if (++ksW >= (int) ks.size()) ksW = 0;
        clavPeak = juce::jmax (clavPeak * 0.9995f, std::abs (y));
        return out * 0.8f;
    }

    float currentLevel() noexcept
    {
        if (mdl == model::TineEP || mdl == model::ReedEP) return fmAmp;
        if (mdl == model::Clav) return clavPeak;
        float m = 0.0f;
        for (int k = 0; k < numPartials; ++k)
            m = juce::jmax (m, pc[(size_t) k] * pc[(size_t) k] + ps[(size_t) k] * ps[(size_t) k]);
        return std::sqrt (m);
    }

    float sr = 44100.0f;
    uint32_t rng = 1u;
    bool active = false, held = false, sustained = false, damped = false;
    int note = 60, mdl = 0, checkCounter = 0;
    float vel = 1.0f, bright = 0.5f, velGain = 1.0f, outGain = 1.0f, dampCoef = 1.0f, peak = 1.0f;
    float panL = 1.0f, panR = 1.0f;
    uint64_t age = 0;

    // modal
    int numPartials = 0;
    std::array<float, kMaxPartials> pc {}, ps {}, pcw {}, psw {}, pgl {}, pgr {};

    // FM
    bool isReed = false;
    float carPhase = 0, modPhase = 0, pingPhase = 0, carInc = 0, modInc = 0, pingInc = 0;
    float index = 0, indexFloor = 0, indexDecay = 1, pingAmp = 0, pingDecay = 1, fmAmp = 0, fmDecay = 1, bark = 0;
    float dcX = 0, dcY = 0;

    // Karplus-Strong
    std::vector<float> ks;
    int ksW = 0;
    float ksDelay = 100, ksGain = 0.99f, ksFreq = 100, pickDelay = 10, clavPeak = 1.0f;

    // noise
    float noiseEnv = 0, noiseCoef = 0.1f, noiseDecay = 0.99f, noiseLp = 0;
};
} // namespace keys
