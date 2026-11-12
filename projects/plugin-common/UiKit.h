#pragma once

// Shared look and controls for Jae's plugins (Ruminator, 82 Keys, Halo).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace jae::ui
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

using APVTS = juce::AudioProcessorValueTreeState;

juce::Font uiFont (float size, bool bold = false);
void layoutGrid (juce::Rectangle<int> r, const std::vector<juce::Component*>& items, int cols);

template <typename T>
std::vector<juce::Component*> comps (const std::vector<T*>& v)
{
    return std::vector<juce::Component*> (v.begin(), v.end());
}

struct Section
{
    juce::String title;
    juce::Rectangle<int> area;
};

void paintSections (juce::Graphics&, const std::vector<Section>&, juce::Colour accent);
void paintTitle (juce::Graphics&, const juce::String& title, const juce::String& subtitle,
                 juce::Colour a, juce::Colour b);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (juce::Colour accent = colours::accent, juce::Colour accent2 = colours::accent2);
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    juce::Colour acc, acc2;
};

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

// Owns child components and keeps them visible; handy base for editors.
struct ComponentBin
{
    template <typename T, typename... Args>
    T& make (juce::Component& parent, Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto& ref = *c;
        parent.addAndMakeVisible (ref);
        owned.push_back (std::move (c));
        return ref;
    }

    std::vector<std::unique_ptr<juce::Component>> owned;
};

// Preset helpers shared by the newer plugins.
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // param id -> real value (choice = index)
};

inline void applyPreset (APVTS& state, const Preset& preset)
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [pid, value] : preset.values)
        if (auto* rp = state.getParameter (pid))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
        else
            jassertfalse; // typo in preset id
}
} // namespace jae::ui
