#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace jae
{
constexpr int kTableSize = 2048;   // samples per frame
constexpr int kNumFrames = 16;     // morph frames per wavetable ("Position")
constexpr int kNumLevels = 11;     // band-limited copies: level L keeps harmonics <= 1024 >> L

juce::StringArray tableNames();

struct Wavetable
{
    std::vector<float> data; // [frame][level][kTableSize + 1]

    const float* get (int frame, int level) const noexcept
    {
        return data.data() + ((size_t) frame * kNumLevels + (size_t) level) * (kTableSize + 1);
    }
};

// Built once, shared by every plugin instance.
class WavetableBank
{
public:
    static const WavetableBank& get();

    const Wavetable& table (int index) const noexcept
    {
        return tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, index)];
    }

    int size() const noexcept { return (int) tables.size(); }

    // Which band-limited level to use for a given phase increment (cycles per sample).
    static int levelForIncrement (float inc) noexcept;

private:
    WavetableBank();
    std::vector<Wavetable> tables;
};
} // namespace jae
