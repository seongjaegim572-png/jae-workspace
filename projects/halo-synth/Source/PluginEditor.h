#pragma once

#include "PluginProcessor.h"
#include "UiKit.h"

// Waveform of the layer's current source, with the grain position and spray range.
class LayerView : public juce::Component, private juce::Timer
{
public:
    LayerView (HaloProcessor& p, int layerIndex);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    HaloProcessor& proc;
    int layer;
    juce::String lastKey;
};

class HaloEditor : public juce::AudioProcessorEditor,
                   public juce::FileDragAndDropTarget,
                   private juce::Timer
{
public:
    explicit HaloEditor (HaloProcessor&);
    ~HaloEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void refreshPresetBox();
    void loadFile (const juce::File& f);
    jae::ui::Knob& knob (const juce::String& pid, const juce::String& text);

    HaloProcessor& proc;
    jae::ui::LookAndFeel lnf { juce::Colour (0xff38bdf8), juce::Colour (0xffa78bfa) };
    jae::ui::ComponentBin bin;
    std::vector<jae::ui::Section> sections;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, loadButton { "Load Sample" };
    juce::Label sampleLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<jae::ui::Knob*> topKnobs;

    struct LayerUI
    {
        jae::ui::Toggle* on;
        jae::ui::Choice *source, *table, *texture, *filter;
        LayerView* view;
        std::vector<jae::ui::Knob*> knobs;
        std::vector<jae::ui::Knob*> wtOnly, granOnly;
    };
    std::array<LayerUI, halo::kLayers> layerUI {};

    struct LfoUI { jae::ui::Choice* shape; jae::ui::Knob* rate; };
    std::array<LfoUI, 2> lfoUI {};
    std::vector<jae::ui::Knob*> modEnvKnobs, macroKnobs;

    jae::ui::Toggle* arpOn = nullptr;
    jae::ui::Choice *arpMode = nullptr, *arpRate = nullptr;
    std::vector<jae::ui::Knob*> arpKnobs;

    struct SlotUI { jae::ui::Choice *src, *dst; jae::ui::AmountBar* amt; };
    std::array<SlotUI, halo::kSlots> slotUI {};

    struct FxUI { jae::ui::Toggle* on; jae::ui::Choice* choice = nullptr; std::vector<jae::ui::Knob*> knobs; };
    std::array<FxUI, 4> fxUI {};
    int shownProgram = -1;
    bool dragOver = false;
};
