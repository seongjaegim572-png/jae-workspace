#pragma once

#include "PluginProcessor.h"
#include "UiKit.h"

class KeysEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KeysEditor (KeysProcessor&);
    ~KeysEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetBox();
    jae::ui::Knob& knob (const juce::String& pid, const juce::String& text);

    KeysProcessor& proc;
    jae::ui::LookAndFeel lnf { juce::Colour (0xfff59e0b), juce::Colour (0xfffb7185) };
    jae::ui::ComponentBin bin;
    std::vector<jae::ui::Section> sections;
    juce::Rectangle<int> keyboardArea;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    jae::ui::Knob* masterKnob = nullptr;
    jae::ui::Choice* modelChoice = nullptr;
    std::vector<jae::ui::Knob*> mainKnobs, tremKnobs;

    struct FxUI { jae::ui::Toggle* on; jae::ui::Choice* choice = nullptr; std::vector<jae::ui::Knob*> knobs; };
    std::array<FxUI, 4> fxUI {};
    int shownProgram = -1;
};
