#include "Textures.h"
#include "DspBits.h"
#include <cmath>

namespace halo
{
juce::StringArray textureNames()
{
    return { "Glass Choir", "Air", "Bowed", "Shimmer", "Dusty Tape", "User Sample" };
}

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kLen = (int) (kSr * 3.0);
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    float noteHz (float n) { return 440.0f * std::exp2 ((n - 69.0f) / 12.0f); }

    void normalise (std::vector<float>& d)
    {
        float peak = 1.0e-6f;
        for (auto s : d) peak = juce::jmax (peak, std::abs (s));
        for (auto& s : d) s *= 0.9f / peak;
    }

    SampleData make (const char* name, float root)
    {
        SampleData s;
        s.name = name;
        s.rootNote = root;
        s.sampleRate = kSr;
        s.data.assign ((size_t) kLen, 0.0f);
        return s;
    }

    // Vowel-like choir: 3 detuned voices, harmonics shaped by moving formants.
    SampleData glassChoir()
    {
        auto s = make ("Glass Choir", 57.0f);
        const float f0 = noteHz (57.0f);
        const float detunes[] = { 0.0f, 0.007f, -0.006f };
        jae::XorShift rng { 77u };
        for (int v = 0; v < 3; ++v)
        {
            float phase[32] {};
            for (int i = 0; i < kLen; ++i)
            {
                const float t = (float) i / (float) kLen;
                const float vib = 1.0f + 0.004f * std::sin (twoPi * (5.0f + (float) v * 0.3f) * (float) i / (float) kSr);
                const float f = f0 * (1.0f + detunes[v]) * vib;
                const float F1 = 700.0f - 250.0f * t, F2 = 1200.0f - 350.0f * t;
                float y = 0.0f;
                for (int k = 1; k <= 30; ++k)
                {
                    const float fk = f * (float) k;
                    if (fk > 9000.0f) break;
                    auto pk = [fk] (float c, float w) { const float d = (fk - c) / w; return std::exp (-d * d); };
                    const float a = 0.05f / (float) k + pk (F1, 110.0f) + 0.6f * pk (F2, 150.0f) + 0.25f * pk (2700.0f, 250.0f);
                    phase[k] += fk / (float) kSr;
                    if (phase[k] >= 1.0f) phase[k] -= 1.0f;
                    y += a * std::sin (twoPi * phase[k]);
                }
                s.data[(size_t) i] += y;
            }
        }
        float lp = 0.0f; // breath
        for (int i = 0; i < kLen; ++i)
        {
            lp += 0.08f * (rng.nextBipolar() - lp);
            s.data[(size_t) i] += lp * 0.6f;
        }
        normalise (s.data);
        return s;
    }

    // Wind: noise through two slowly sweeping resonant band-passes.
    SampleData air()
    {
        auto s = make ("Air", 60.0f);
        jae::XorShift rng { 11u };
        jae::Svf a, b;
        jae::SvfCoefs ca, cb;
        for (int i = 0; i < kLen; ++i)
        {
            const float t = (float) i / (float) kSr;
            if ((i & 31) == 0)
            {
                const float fa = 600.0f * std::exp2 (1.5f * std::sin (twoPi * 0.23f * t));
                const float fb = 2400.0f * std::exp2 (1.0f * std::sin (twoPi * 0.17f * t + 1.0f));
                ca.set (std::tan (juce::MathConstants<float>::pi * fa / (float) kSr), 0.25f);
                cb.set (std::tan (juce::MathConstants<float>::pi * fb / (float) kSr), 0.4f);
            }
            const float n = rng.nextBipolar();
            s.data[(size_t) i] = a.process (n, ca, 1) + 0.6f * b.process (n, cb, 1);
        }
        normalise (s.data);
        return s;
    }

    // Bowed string: band-limited saw with vibrato, bow-pressure swell and bow noise.
    SampleData bowed()
    {
        auto s = make ("Bowed", 48.0f);
        const float f0 = noteHz (48.0f);
        jae::XorShift rng { 5u };
        float phase[40] {};
        float lp = 0.0f;
        for (int i = 0; i < kLen; ++i)
        {
            const float t = (float) i / (float) kSr;
            const float f = f0 * (1.0f + 0.005f * std::sin (twoPi * 5.2f * t));
            const float swell = 0.7f + 0.3f * std::sin (twoPi * 0.4f * t);
            float y = 0.0f;
            for (int k = 1; k <= 36; ++k)
            {
                const float fk = f * (float) k;
                if (fk > 12000.0f) break;
                phase[k] += fk / (float) kSr;
                if (phase[k] >= 1.0f) phase[k] -= 1.0f;
                y += std::sin (twoPi * phase[k]) / (float) k * (k > 8 ? swell : 1.0f);
            }
            lp += 0.2f * (rng.nextBipolar() - lp);
            s.data[(size_t) i] = y * swell + lp * 0.08f;
        }
        normalise (s.data);
        return s;
    }

    // Shimmer: inharmonic bell partials with independent slow tremolo.
    SampleData shimmer()
    {
        auto s = make ("Shimmer", 72.0f);
        const float f0 = noteHz (72.0f);
        const float ratios[] = { 1.0f, 2.01f, 3.99f, 5.43f, 7.9f, 11.2f, 14.7f };
        const float rates[] = { 0.3f, 0.7f, 1.1f, 0.5f, 1.7f, 2.3f, 0.9f };
        float phase[7] {};
        for (int i = 0; i < kLen; ++i)
        {
            const float t = (float) i / (float) kSr;
            float y = 0.0f;
            for (int k = 0; k < 7; ++k)
            {
                phase[k] += f0 * ratios[k] / (float) kSr;
                if (phase[k] >= 1.0f) phase[k] -= 1.0f;
                const float trem = 0.55f + 0.45f * std::sin (twoPi * rates[k] * t + (float) k);
                y += std::sin (twoPi * phase[k]) * trem / std::pow ((float) k + 1.0f, 0.7f);
            }
            s.data[(size_t) i] = y;
        }
        normalise (s.data);
        return s;
    }

    // Dusty tape: warm drone with wow, hiss and vinyl crackle.
    SampleData dustyTape()
    {
        auto s = make ("Dusty Tape", 60.0f);
        const float f0 = noteHz (60.0f);
        jae::XorShift rng { 99u };
        float ph1 = 0.0f, ph2 = 0.0f, ph3 = 0.0f, hissLp = 0.0f, crackle = 0.0f;
        for (int i = 0; i < kLen; ++i)
        {
            const float t = (float) i / (float) kSr;
            const float wow = 1.0f + 0.003f * std::sin (twoPi * 0.6f * t);
            ph1 += f0 * wow / (float) kSr;        if (ph1 >= 1.0f) ph1 -= 1.0f;
            ph2 += f0 * 2.0f * wow / (float) kSr; if (ph2 >= 1.0f) ph2 -= 1.0f;
            ph3 += f0 * 3.0f * wow / (float) kSr; if (ph3 >= 1.0f) ph3 -= 1.0f;
            float y = std::sin (twoPi * ph1) + 0.35f * std::sin (twoPi * ph2) + 0.15f * std::sin (twoPi * ph3);
            y = std::tanh (y * 1.3f);
            hissLp += 0.3f * (rng.nextBipolar() - hissLp);
            if (rng.next01() < 0.0004f) crackle = rng.nextBipolar();
            crackle *= 0.9f;
            s.data[(size_t) i] = y + hissLp * 0.05f + crackle * 0.5f;
        }
        normalise (s.data);
        return s;
    }
}

const TextureBank& TextureBank::get()
{
    static const TextureBank bank;
    return bank;
}

TextureBank::TextureBank()
{
    textures.push_back (glassChoir());
    textures.push_back (air());
    textures.push_back (bowed());
    textures.push_back (shimmer());
    textures.push_back (dustyTape());
}

const SampleData& TextureBank::texture (int index) const noexcept
{
    return textures[(size_t) juce::jlimit (0, (int) textures.size() - 1, index)];
}
} // namespace halo
