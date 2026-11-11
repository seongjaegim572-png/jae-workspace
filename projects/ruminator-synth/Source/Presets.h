#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <vector>

namespace rum
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // param id -> real value (choice = index)
};

const std::vector<Preset>& factoryPresets();

// Resets every parameter to default, then applies the preset's values.
void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& preset);
} // namespace rum
