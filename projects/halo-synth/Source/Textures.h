#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace halo
{
// A mono sound the granular engine plays from.
struct SampleData
{
    juce::String name;
    std::vector<float> data;
    double sampleRate = 48000.0;
    float rootNote = 60.0f;
};

// Texture names shown in the menu. The last one is the user's own sample.
juce::StringArray textureNames();
constexpr int kUserTexture = 5;

// Built-in textures, synthesised once at start-up (no recordings needed).
class TextureBank
{
public:
    static const TextureBank& get();
    const SampleData& texture (int index) const noexcept;
    int size() const noexcept { return (int) textures.size(); }

private:
    TextureBank();
    std::vector<SampleData> textures;
};
} // namespace halo
