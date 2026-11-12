#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "KeysEngine.h"
#include "Effects.h"

class KeysProcessor : public juce::AudioProcessor
{
public:
    KeysProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "82 Keys"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    void readSettings();
    void handleMidi (const juce::MidiMessage& m);
    void renderVoices (float* L, float* R, int start, int end);
    float p (const char* pid) const { return apvts.getRawParameterValue (pid)->load(); }

    static constexpr int kVoices = 24;
    std::array<keys::Voice, kVoices> voices;
    keys::Settings settings;
    jae::FxSettings fx;
    jae::Effects effects;
    juce::SmoothedValue<float> masterGain;
    uint64_t counter = 0;
    bool sustainDown = false;
    float tremPhase = 0.0f;
    double sr = 44100.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeysProcessor)
};
