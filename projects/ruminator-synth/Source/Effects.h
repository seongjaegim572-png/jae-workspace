#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace rum
{
struct FxSettings
{
    bool distOn = false; float distDrive = 0.3f, distMix = 0.5f;
    bool choOn = false;  float choRate = 0.8f, choDepth = 0.4f, choMix = 0.4f;
    bool dlyOn = false;  double dlyBeats = 0.75; float dlyFb = 0.35f, dlyMix = 0.25f;
    bool revOn = true;   float revSize = 0.6f, revDamp = 0.5f, revMix = 0.2f;
};

class PingPongDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sampleRate * 5.0) + 4;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        time.reset (sampleRate, 0.08);
        time.setCurrentAndTargetValue ((float) (sampleRate * 0.25));
        reset();
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0; lpL = lpR = 0.0f;
    }

    void process (float* L, float* R, int n, float delaySamples, float fb, float mix) noexcept
    {
        time.setTargetValue (juce::jlimit (1.0f, (float) (size - 3), delaySamples));
        for (int i = 0; i < n; ++i)
        {
            const float d = time.getNextValue();
            float rp = (float) w - d;
            if (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % size;
            const float fr = rp - (float) i0;
            const float dl = bufL[(size_t) i0] + fr * (bufL[(size_t) i1] - bufL[(size_t) i0]);
            const float dr = bufR[(size_t) i0] + fr * (bufR[(size_t) i1] - bufR[(size_t) i0]);

            // gentle damping in the feedback path
            lpL += 0.45f * (dl - lpL);
            lpR += 0.45f * (dr - lpR);

            const float in = 0.5f * (L[i] + R[i]);
            bufL[(size_t) w] = in + fb * lpR;
            bufR[(size_t) w] = fb * lpL;
            if (++w >= size) w = 0;

            L[i] += mix * dl;
            R[i] += mix * dr;
        }
    }

private:
    double sr = 44100.0;
    int size = 1, w = 0;
    std::vector<float> bufL, bufR;
    float lpL = 0.0f, lpR = 0.0f;
    juce::SmoothedValue<float> time;
};

class Effects
{
public:
    void prepare (double sampleRate, int blockSize)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, blockSize), 2 };
        chorus.prepare (spec);
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.15f);
        reverb.prepare (spec);
        delay.prepare (sampleRate);
    }

    void reset()
    {
        chorus.reset();
        reverb.reset();
        delay.reset();
    }

    void process (juce::AudioBuffer<float>& buffer, const FxSettings& s, double bpm) noexcept
    {
        const int n = buffer.getNumSamples();
        if (buffer.getNumChannels() < 2 || n == 0) return;
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);

        if (s.distOn)
        {
            const float k = 1.0f + s.distDrive * 24.0f;
            const float norm = 1.0f / std::sqrt (k);
            for (int i = 0; i < n; ++i)
            {
                L[i] += s.distMix * (std::tanh (L[i] * k) * norm - L[i]);
                R[i] += s.distMix * (std::tanh (R[i] * k) * norm - R[i]);
            }
        }

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        if (s.choOn)
        {
            chorus.setRate (s.choRate);
            chorus.setDepth (s.choDepth);
            chorus.setMix (s.choMix);
            chorus.process (ctx);
        }

        if (s.dlyOn)
        {
            const double safeBpm = bpm > 20.0 ? bpm : 120.0;
            const float samples = (float) (s.dlyBeats * 60.0 / safeBpm * sr);
            delay.process (L, R, n, samples, s.dlyFb, s.dlyMix);
        }

        if (s.revOn)
        {
            juce::dsp::Reverb::Parameters p;
            p.roomSize = s.revSize;
            p.damping = s.revDamp;
            p.wetLevel = s.revMix;
            p.dryLevel = 1.0f - s.revMix * 0.5f;
            p.width = 1.0f;
            reverb.setParameters (p);
            reverb.process (ctx);
        }
    }

private:
    double sr = 44100.0;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    PingPongDelay delay;
};
} // namespace rum
