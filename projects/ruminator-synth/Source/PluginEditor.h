#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace rum::ui
{
namespace colours
{
    const juce::Colour bg       { 0xff101016 };
    const juce::Colour panel    { 0xff1a1a24 };
    const juce::Colour panelHi  { 0xff242432 };
    const juce::Colour accent   { 0xff8b5cf6 };
    const juce::Colour accent2  { 0xff22d3ee };
    const juce::Colour text     { 0xffe5e7eb };
    const juce::Colour dim      { 0xff8b8b9a };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

using APVTS = juce::AudioProcessorValueTreeState;

struct Knob : juce::Component
{
    Knob (APVTS& s, const juce::String& pid, const juce::String& text, juce::Component* popupParent);
    void resized() override;

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

struct Choice : juce::Component
{
    Choice (APVTS& s, const juce::String& pid, const juce::String& text = {});
    void resized() override;

    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

struct Toggle : juce::Component
{
    Toggle (APVTS& s, const juce::String& pid, const juce::String& text);
    void resized() override { button.setBounds (getLocalBounds()); }

    juce::ToggleButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

struct AmountBar : juce::Component
{
    AmountBar (APVTS& s, const juce::String& pid, juce::Component* popupParent);
    void resized() override { slider.setBounds (getLocalBounds()); }

    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

class WavetableView : public juce::Component, private juce::Timer
{
public:
    WavetableView (APVTS& s, int oscIndex);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::atomic<float>* table;
    std::atomic<float>* pos;
    std::atomic<float>* on;
    float lastTable = -1.0f, lastPos = -1.0f, lastOn = -1.0f;
};
} // namespace rum::ui

class RuminatorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RuminatorEditor (RuminatorProcessor&);
    ~RuminatorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetBox();

    template <typename T, typename... Args>
    T& make (Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto& ref = *c;
        addAndMakeVisible (ref);
        owned.push_back (std::move (c));
        return ref;
    }

    rum::ui::Knob& knob (const juce::String& pid, const juce::String& text) { return make<rum::ui::Knob> (proc.apvts, pid, text, this); }

    RuminatorProcessor& proc;
    rum::ui::LookAndFeel lnf;
    std::vector<std::unique_ptr<juce::Component>> owned;

    struct Section { juce::String title; juce::Rectangle<int> area; };
    std::vector<Section> sections;

    // Top bar
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    struct OscUI
    {
        rum::ui::Toggle* on; rum::ui::Choice* table; rum::ui::WavetableView* view;
        std::vector<rum::ui::Knob*> knobs;
    };
    std::array<OscUI, rum::kNumOscs> oscUI {};

    std::vector<rum::ui::Knob*> topKnobs, subKnobs, filterKnobs, ampKnobs, modKnobs, macroKnobs;
    rum::ui::Toggle* monoToggle = nullptr;
    rum::ui::Choice* subShape = nullptr;
    rum::ui::Choice* filterType = nullptr;

    struct LfoUI { rum::ui::Choice* shape; rum::ui::Toggle* sync; rum::ui::Knob* rate; rum::ui::Choice* div; };
    std::array<LfoUI, rum::kNumLfos> lfoUI {};

    struct SlotUI { rum::ui::Choice* src; rum::ui::Choice* dst; rum::ui::AmountBar* amt; };
    std::array<SlotUI, rum::kNumSlots> slotUI {};

    struct FxUI { rum::ui::Toggle* on; rum::ui::Choice* choice = nullptr; std::vector<rum::ui::Knob*> knobs; };
    std::array<FxUI, 4> fxUI {};

    int shownProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RuminatorEditor)
};
