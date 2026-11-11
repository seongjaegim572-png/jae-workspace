#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Voice.h"
#include "Effects.h"

class RuminatorProcessor : public juce::AudioProcessor
{
public:
    RuminatorProcessor();
    ~RuminatorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ruminator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    void readSettings (double bpm);
    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();
    void renderVoices (float* L, float* R, int start, int end);

    std::atomic<float>* raw (const juce::String& pid) { return apvts.getRawParameterValue (pid); }

    struct OscPtrs { std::atomic<float> *on, *table, *pos, *coarse, *fine, *level, *unison, *detune, *spread, *warp, *pan; };
    std::array<OscPtrs, rum::kNumOscs> oscP {};
    std::atomic<float> *subLevel, *subShape, *noiseLevel;
    std::atomic<float> *fltType, *cutoff, *res, *drive, *fltEnv, *fltKey;
    std::atomic<float> *ampA, *ampD, *ampS, *ampR, *modA, *modD, *modS, *modR;
    std::array<std::atomic<float>*, rum::kNumLfos> lfoShapeP {}, lfoRateP {}, lfoSyncP {}, lfoDivP {};
    std::array<std::atomic<float>*, rum::kNumSlots> slotSrcP {}, slotDstP {}, slotAmtP {};
    std::array<std::atomic<float>*, rum::kNumMacros> macroP {};
    std::atomic<float> *distOn, *distDrive, *distMix, *choOn, *choRate, *choDepth, *choMix;
    std::atomic<float> *dlyOn, *dlyDiv, *dlyFb, *dlyMix, *revOn, *revSize, *revDamp, *revMix;
    std::atomic<float> *master, *glide, *mono, *bendRange, *velSens;

    rum::SynthSettings settings;
    rum::FxSettings fx;
    rum::ControllerState ctrl;
    float bendNorm = 0.0f;
    bool sustainDown = false;

    std::array<rum::Voice, rum::kMaxVoices> voices;
    uint64_t noteCounter = 0;
    int lastNote = -1;
    std::vector<int> heldNotes;

    rum::Effects effects;
    juce::SmoothedValue<float> masterGain;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RuminatorProcessor)
};
