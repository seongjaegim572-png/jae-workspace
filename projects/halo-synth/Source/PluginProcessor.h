#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "HaloVoice.h"
#include "Effects.h"

class HaloProcessor : public juce::AudioProcessor
{
public:
    HaloProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Halo"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Message thread only. Returns false if the file can't be read.
    bool loadUserSample (const juce::File& file);
    juce::String getUserSampleName() const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    void readSettings();
    void handleMidi (const juce::MidiMessage& m);
    void voiceOn (int note, float vel);
    void voiceOff (int note);
    void arpStep();
    void renderVoices (float* L, float* R, int start, int end);
    float p (const juce::String& pid) const { return apvts.getRawParameterValue (pid)->load(); }

    static constexpr int kVoices = 12;
    std::array<halo::Voice, kVoices> voices;
    halo::Settings settings;
    jae::FxSettings fx;
    jae::Effects effects;
    juce::SmoothedValue<float> masterGain;
    double sr = 44100.0, bpm = 120.0;
    uint64_t counter = 0;
    int currentProgram = 0;
    bool sustainDown = false;
    std::vector<int> sustainedNotes;

    // arpeggiator
    struct ArpNote { int note; float vel; };
    std::vector<ArpNote> arpHeld;
    int arpIndex = 0, arpPlaying = -1;
    float arpVel = 0.8f;
    juce::int64 samplesToNext = 0, samplesToOff = -1;
    bool arpWasOn = false;
    juce::Random arpRandom;

    // user sample (kept alive while the audio thread may still read it)
    std::vector<std::shared_ptr<halo::SampleData>> loadedSamples;
    std::atomic<const halo::SampleData*> userSample { nullptr };
    juce::File userSampleFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaloProcessor)
};
