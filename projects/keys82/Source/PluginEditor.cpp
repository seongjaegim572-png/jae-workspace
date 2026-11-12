#include "PluginEditor.h"

using namespace jae::ui;

KeysEditor::KeysEditor (KeysProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) proc.setCurrentProgram (idx);
    };
    auto step = [this] (int d)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
        refreshPresetBox();
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (1); };

    masterKnob = &knob ("master", "Master");
    modelChoice = &bin.make<Choice> (*this, s, "model");
    mainKnobs = { &knob ("tone", "Tone"), &knob ("hardness", "Hardness"), &knob ("decay", "Decay"),
                  &knob ("release", "Release"), &knob ("noise", "Mechanics"), &knob ("wear", "Wear"),
                  &knob ("drive", "Drive"), &knob ("width", "Width"), &knob ("vel_sens", "Velocity") };
    tremKnobs = { &knob ("trem_rate", "Rate"), &knob ("trem_depth", "Depth") };

    fxUI[0].on = &bin.make<Toggle> (*this, s, "fx_phs_on", "PHASER");
    fxUI[0].knobs = { &knob ("fx_phs_rate", "Rate"), &knob ("fx_phs_depth", "Depth"), &knob ("fx_phs_mix", "Mix") };
    fxUI[1].on = &bin.make<Toggle> (*this, s, "fx_cho_on", "CHORUS");
    fxUI[1].knobs = { &knob ("fx_cho_rate", "Rate"), &knob ("fx_cho_depth", "Depth"), &knob ("fx_cho_mix", "Mix") };
    fxUI[2].on = &bin.make<Toggle> (*this, s, "fx_dly_on", "DELAY");
    fxUI[2].choice = &bin.make<Choice> (*this, s, "fx_dly_div");
    fxUI[2].knobs = { &knob ("fx_dly_fb", "Feedback"), &knob ("fx_dly_mix", "Mix") };
    fxUI[3].on = &bin.make<Toggle> (*this, s, "fx_rev_on", "REVERB");
    fxUI[3].knobs = { &knob ("fx_rev_size", "Size"), &knob ("fx_rev_damp", "Damp"), &knob ("fx_rev_mix", "Mix") };

    setSize (1000, 600);
    startTimerHz (10);
}

KeysEditor::~KeysEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

Knob& KeysEditor::knob (const juce::String& pid, const juce::String& text)
{
    return bin.make<Knob> (*this, proc.apvts, pid, text, this);
}

void KeysEditor::refreshPresetBox()
{
    const int n = proc.getNumPrograms();
    if (presetBox.getNumItems() != n)
    {
        presetBox.clear (juce::dontSendNotification);
        for (int i = 0; i < n; ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    }
    shownProgram = proc.getCurrentProgram();
    presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
}

void KeysEditor::timerCallback()
{
    if (proc.getCurrentProgram() != shownProgram) refreshPresetBox();
}

void KeysEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    paintTitle (g, "82 KEYS", "modeled keys  |  82 Jae", lnf.acc, lnf.acc2);
    paintSections (g, sections, lnf.acc);

    // decorative keyboard
    if (! keyboardArea.isEmpty())
    {
        const auto r = keyboardArea.toFloat();
        constexpr int whites = 36;
        const float kw = r.getWidth() / (float) whites;
        for (int i = 0; i < whites; ++i)
        {
            g.setColour (colours::text.withAlpha (0.9f));
            g.fillRoundedRectangle (r.getX() + (float) i * kw + 1.0f, r.getY(), kw - 2.0f, r.getHeight(), 2.0f);
        }
        static const bool hasBlack[] = { true, true, false, true, true, true, false };
        for (int i = 0; i < whites - 1; ++i)
            if (hasBlack[i % 7])
            {
                g.setColour (colours::bg);
                g.fillRoundedRectangle (r.getX() + ((float) i + 0.68f) * kw, r.getY(), kw * 0.64f, r.getHeight() * 0.6f, 2.0f);
            }
        g.setGradientFill (juce::ColourGradient (lnf.acc.withAlpha (0.0f), r.getX(), 0.0f, lnf.acc.withAlpha (0.35f), r.getRight(), 0.0f, false));
        g.fillRect (r.withHeight (3.0f));
    }
}

void KeysEditor::resized()
{
    sections.clear();
    auto area = getLocalBounds().reduced (10);
    constexpr int gap = 8;
    auto section = [this] (const juce::String& t, juce::Rectangle<int> r)
    {
        sections.push_back ({ t, r });
        return r.reduced (8).withTrimmedTop (20);
    };

    auto top = area.removeFromTop (58);
    top.removeFromLeft (250);
    auto presetArea = top.removeFromLeft (320).withSizeKeepingCentre (320, 28);
    prevButton.setBounds (presetArea.removeFromLeft (28));
    nextButton.setBounds (presetArea.removeFromRight (28));
    presetBox.setBounds (presetArea.reduced (4, 0));
    masterKnob->setBounds (top.removeFromRight (70));

    area.removeFromTop (gap);
    auto row1 = area.removeFromTop (250);
    {
        auto c = section ("INSTRUMENT", row1.removeFromLeft (row1.getWidth() - 220 - gap));
        row1.removeFromLeft (gap);
        modelChoice->setBounds (c.removeFromTop (28).removeFromLeft (260));
        c.removeFromTop (6);
        keyboardArea = c.removeFromBottom (34);
        c.removeFromBottom (6);
        layoutGrid (c, comps (mainKnobs), 9);
    }
    layoutGrid (section ("TREMOLO", row1), comps (tremKnobs), 2);

    area.removeFromTop (gap);
    auto c = section ("FX", area);
    const int colW = (c.getWidth() - 3 * gap) / 4;
    for (auto& ui : fxUI)
    {
        auto col = c.removeFromLeft (colW);
        c.removeFromLeft (gap);
        ui.on->setBounds (col.removeFromTop (24));
        col.removeFromTop (6);
        if (ui.choice != nullptr)
        {
            ui.choice->setBounds (col.removeFromTop (24));
            col.removeFromTop (4);
        }
        layoutGrid (col, comps (ui.knobs), (int) ui.knobs.size());
    }
}
