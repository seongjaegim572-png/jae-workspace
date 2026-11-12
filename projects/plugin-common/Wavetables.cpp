#include "Wavetables.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace jae
{
juce::StringArray tableNames()
{
    return { "Basic", "PWM", "Sync", "Harmonic", "Formant", "Digital", "Organ", "Fold" };
}

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    float frac (float x) { return x - std::floor (x); }

    float shapeSine (float p)   { return std::sin (twoPi * p); }
    float shapeTri (float p)    { return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f); }
    float shapeSaw (float p)    { return 1.0f - 2.0f * p; }
    float shapeSquare (float p) { return p < 0.5f ? 1.0f : -1.0f; }

    // Fills one frame (naive, not band-limited yet). t = morph position 0..1.
    void generate (int table, float t, std::vector<float>& out)
    {
        const int n = (int) out.size();

        switch (table)
        {
            case 0: // Basic: sine -> triangle -> saw -> square
            {
                const float seg = t * 3.0f;
                const int s = juce::jmin (2, (int) seg);
                const float x = seg - (float) s;
                for (int i = 0; i < n; ++i)
                {
                    const float p = (float) i / (float) n;
                    float a, b;
                    if (s == 0)      { a = shapeSine (p); b = shapeTri (p); }
                    else if (s == 1) { a = shapeTri (p);  b = shapeSaw (p); }
                    else             { a = shapeSaw (p);  b = shapeSquare (p); }
                    out[(size_t) i] = a + x * (b - a);
                }
                break;
            }
            case 1: // PWM: 50% -> 4% pulse
            {
                const float w = 0.5f - 0.46f * t;
                for (int i = 0; i < n; ++i)
                    out[(size_t) i] = ((float) i / (float) n) < w ? 1.0f : -1.0f;
                break;
            }
            case 2: // Hard sync saw sweep
            {
                const float ratio = 1.0f + t * 7.0f;
                for (int i = 0; i < n; ++i)
                    out[(size_t) i] = 1.0f - 2.0f * frac ((float) i / (float) n * ratio);
                break;
            }
            case 3: // Harmonic build-up
            {
                const float count = 1.0f + t * 47.0f;
                const int full = (int) count;
                const float part = count - (float) full;
                for (int i = 0; i < n; ++i)
                {
                    const float p = (float) i / (float) n;
                    float y = 0.0f;
                    for (int k = 1; k <= full; ++k)
                        y += std::sin (twoPi * (float) k * p) / (float) k;
                    y += part * std::sin (twoPi * (float) (full + 1) * p) / (float) (full + 1);
                    out[(size_t) i] = y;
                }
                break;
            }
            case 4: // Formant: vowels A E I O U
            {
                static const float f1[] = { 730.0f, 530.0f, 270.0f, 570.0f, 300.0f };
                static const float f2[] = { 1090.0f, 1840.0f, 2290.0f, 840.0f, 870.0f };
                const float v = t * 4.0f;
                const int a = juce::jmin (3, (int) v);
                const float x = v - (float) a;
                const float F1 = f1[a] + x * (f1[a + 1] - f1[a]);
                const float F2 = f2[a] + x * (f2[a + 1] - f2[a]);
                const float f0 = 110.0f;
                std::fill (out.begin(), out.end(), 0.0f);
                for (int k = 1; (float) k * f0 < 9000.0f; ++k)
                {
                    const float f = (float) k * f0;
                    auto peak = [f] (float c, float w) { const float d = (f - c) / w; return std::exp (-d * d); };
                    const float amp = 0.08f / (float) k + peak (F1, 90.0f) + 0.7f * peak (F2, 130.0f)
                                      + 0.25f * peak (2800.0f, 220.0f);
                    for (int i = 0; i < n; ++i)
                        out[(size_t) i] += amp * std::sin (twoPi * (float) k * (float) i / (float) n);
                }
                break;
            }
            case 5: // Digital: random spectra + bit reduction
            {
                constexpr int keys = 5, harms = 48;
                static float amps[keys][harms];
                static float phases[harms];
                static bool init = false;
                if (! init)
                {
                    juce::Random rng (82);
                    for (int k = 0; k < keys; ++k)
                        for (int h = 0; h < harms; ++h)
                        {
                            const float r = rng.nextFloat();
                            amps[k][h] = r * r * r / std::sqrt ((float) h + 1.0f);
                        }
                    for (int h = 0; h < harms; ++h) phases[h] = rng.nextFloat();
                    amps[0][0] = 1.0f;
                    init = true;
                }
                const float v = t * (float) (keys - 1);
                const int a = juce::jmin (keys - 2, (int) v);
                const float x = v - (float) a;
                std::fill (out.begin(), out.end(), 0.0f);
                for (int h = 0; h < harms; ++h)
                {
                    const float amp = amps[a][h] + x * (amps[a + 1][h] - amps[a][h]);
                    for (int i = 0; i < n; ++i)
                        out[(size_t) i] += amp * std::sin (twoPi * ((float) (h + 1) * (float) i / (float) n + phases[h]));
                }
                float peak = 1.0e-6f;
                for (auto s : out) peak = juce::jmax (peak, std::abs (s));
                const float steps = 16.0f - t * 12.0f;
                for (auto& s : out) s = std::round (s / peak * steps) / steps;
                break;
            }
            case 6: // Organ drawbars
            {
                static const int h[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                constexpr int bars = 9;
                std::fill (out.begin(), out.end(), 0.0f);
                for (int j = 0; j < bars; ++j)
                {
                    const float w = juce::jlimit (0.0f, 1.0f, t * (float) bars - (float) j + 1.0f) / (1.0f + 0.2f * (float) j);
                    if (w <= 0.0f) continue;
                    for (int i = 0; i < n; ++i)
                        out[(size_t) i] += w * std::sin (twoPi * (float) h[j] * (float) i / (float) n);
                }
                break;
            }
            default: // Fold: sine wavefolder
            {
                const float g = 0.5f + t * 8.0f;
                for (int i = 0; i < n; ++i)
                    out[(size_t) i] = std::sin (g * std::sin (twoPi * (float) i / (float) n));
                break;
            }
        }
    }
}

int WavetableBank::levelForIncrement (float inc) noexcept
{
    // Highest harmonic that still fits under Nyquist.
    const float maxHarm = 0.5f / juce::jmax (1.0e-7f, inc);
    if (maxHarm >= (float) (kTableSize / 2)) return 0;
    const int level = (int) std::ceil (std::log2 ((float) (kTableSize / 2) / maxHarm));
    return juce::jlimit (0, kNumLevels - 1, level);
}

const WavetableBank& WavetableBank::get()
{
    static const WavetableBank bank;
    return bank;
}

WavetableBank::WavetableBank()
{
    constexpr int N = kTableSize;
    juce::dsp::FFT fft (11); // 2^11 = 2048
    static_assert (N == 2048, "FFT order must match table size");

    std::vector<float> naive ((size_t) N), spectrum ((size_t) N * 2), work ((size_t) N * 2);
    const int numTables = tableNames().size();

    for (int t = 0; t < numTables; ++t)
    {
        Wavetable wt;
        wt.data.resize ((size_t) kNumFrames * kNumLevels * (N + 1));

        for (int f = 0; f < kNumFrames; ++f)
        {
            generate (t, (float) f / (float) (kNumFrames - 1), naive);

            std::fill (spectrum.begin(), spectrum.end(), 0.0f);
            std::copy (naive.begin(), naive.end(), spectrum.begin());
            fft.performRealOnlyForwardTransform (spectrum.data());

            float gain = 1.0f;
            for (int level = 0; level < kNumLevels; ++level)
            {
                const int maxHarm = (N / 2) >> level;
                work = spectrum;
                work[0] = work[1] = 0.0f; // remove DC
                for (int k = 1; k < N; ++k)
                {
                    const int h = k <= N / 2 ? k : N - k;
                    if (h > maxHarm || h == N / 2)
                        work[(size_t) (2 * k)] = work[(size_t) (2 * k + 1)] = 0.0f;
                }
                fft.performRealOnlyInverseTransform (work.data());

                if (level == 0)
                {
                    float peak = 0.0f;
                    for (int i = 0; i < N; ++i) peak = juce::jmax (peak, std::abs (work[(size_t) i]));
                    gain = peak > 1.0e-9f ? 1.0f / peak : 1.0f;
                }

                auto* dest = const_cast<float*> (wt.get (f, level));
                for (int i = 0; i < N; ++i) dest[i] = work[(size_t) i] * gain;
                dest[N] = dest[0];
            }
        }
        tables.push_back (std::move (wt));
    }
}
} // namespace jae
