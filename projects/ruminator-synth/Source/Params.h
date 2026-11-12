#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace jae {}

namespace rum
{
using namespace jae; // shared code (wavetables, effects) lives in plugin-common

constexpr int kNumOscs = 2;
constexpr int kNumLfos = 2;
constexpr int kNumSlots = 6;
constexpr int kNumMacros = 4;
constexpr int kMaxUnison = 7;
constexpr int kMaxVoices = 16;

namespace id
{
    inline juce::String osc (int i, const char* name)   { return "osc" + juce::String (i + 1) + "_" + name; }
    inline juce::String lfo (int i, const char* name)   { return "lfo" + juce::String (i + 1) + "_" + name; }
    inline juce::String slot (int i, const char* name)  { return "mm" + juce::String (i + 1) + "_" + name; }
    inline juce::String macro (int i)                   { return "macro" + juce::String (i + 1); }
}

// Modulation sources
namespace src
{
    enum { None, Lfo1, Lfo2, ModEnv, AmpEnv, Velocity, ModWheel, Aftertouch, Keytrack, Random,
           Macro1, Macro2, Macro3, Macro4, Count };
}

// Modulation destinations
namespace dst
{
    enum { None, Osc1Pos, Osc2Pos, Osc1Pitch, Osc2Pitch, AllPitch, Osc1Warp, Osc2Warp,
           Osc1Level, Osc2Level, Osc1Detune, Osc2Detune, Cutoff, Resonance, Drive, Amp, Pan,
           Lfo1Rate, Lfo2Rate, Count };
}

namespace filt
{
    enum { LP12, LP24, BP, HP, Notch };
}

namespace lfoShape
{
    enum { Sine, Triangle, Saw, Square, SampleHold };
}

struct Division
{
    const char* name;
    double beats;
};

juce::StringArray modSourceNames();
juce::StringArray modDestNames();
juce::StringArray filterTypeNames();
juce::StringArray lfoShapeNames();
const std::vector<Division>& lfoDivisions();
const std::vector<Division>& delayDivisions();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace rum
