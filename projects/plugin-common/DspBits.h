#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdint>

namespace jae
{
// Linear attack, exponential decay/release ADSR.
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
            case Attack:  value += attackInc; if (value >= 1.0f) { value = 1.0f; stage = Decay; } break;
            case Decay:   value = sustain + (value - sustain) * decayCoef; break;
            case Release: value *= releaseCoef; if (value < 1.0e-4f) { value = 0.0f; stage = Idle; } break;
            case Idle:
            default:      break;
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

// Zero-delay-feedback state variable filter.
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

    // mode: 0 = low, 1 = band, 2 = high
    inline float process (float v0, const SvfCoefs& c, int mode) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (mode == 0) return v2;
        if (mode == 1) return v1 * c.k;
        return v0 - c.k * v1 - v2;
    }
};

struct XorShift
{
    uint32_t s = 1u;
    float next01() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (s >> 8) * (1.0f / 16777216.0f);
    }
    float nextBipolar() noexcept { return next01() * 2.0f - 1.0f; }
};
} // namespace jae
