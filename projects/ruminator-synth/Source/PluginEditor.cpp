#include "PluginEditor.h"
#include "Wavetables.h"

using namespace rum;

namespace
{
juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

void layoutGrid (juce::Rectangle<int> r, const std::vector<juce::Component*>& items, int cols)
{
    if (items.empty()) return;
    const int rows = ((int) items.size() + cols - 1) / cols;
    const int cw = r.getWidth() / cols, ch = r.getHeight() / rows;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int c = (int) i % cols, row = (int) i / cols;
        items[i]->setBounds (r.getX() + c * cw, r.getY() + row * ch, cw, ch);
    }
}

template <typename T>
std::vector<juce::Component*> comps (const std::vector<T*>& v)
{
    return std::vector<juce::Component*> (v.begin(), v.end());
}
}

namespace rum::ui
{
//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg);
    setColour (juce::Label::textColourId, colours::dim);
    setColour (juce::ComboBox::backgroundColourId, colours::panelHi);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, colours::panelHi);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::BubbleComponent::backgroundColourId, colours::panelHi);
    setColour (juce::BubbleComponent::outlineColourId, colours::accent);
    setColour (juce::TooltipWindow::textColourId, colours::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float arcR = radius - 2.0f;
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;

    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.0f, start, end, true);
    g.setColour (colours::panelHi.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (cx, cy, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setGradientFill (juce::ColourGradient (colours::accent, cx - radius, cy + radius,
                                                 colours::accent2, cx + radius, cy - radius, false));
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = arcR - 5.0f;
    g.setColour (colours::panelHi);
    g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    const juce::Point<float> tip (cx + (bodyR - 2.0f) * std::sin (angle), cy - (bodyR - 2.0f) * std::cos (angle));
    const juce::Point<float> inner (cx + bodyR * 0.35f * std::sin (angle), cy - bodyR * 0.35f * std::cos (angle));
    g.setColour (colours::text);
    g.drawLine ({ inner, tip }, 2.0f);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.0f, 4.0f);
    g.setColour (colours::panelHi);
    g.fillRoundedRectangle (r, 4.0f);

    const float mid = r.getCentreX();
    const auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (mid, pos), r.getY(), juce::jmax (mid, pos), r.getBottom());
    g.setColour (s.getValue() >= 0.0 ? colours::accent : colours::accent2);
    g.fillRoundedRectangle (fill, 3.0f);

    g.setColour (colours::dim.withAlpha (0.6f));
    g.drawVerticalLine ((int) mid, r.getY(), r.getBottom());

    g.setColour (colours::text);
    g.setFont (uiFont (11.0f));
    g.drawText (s.getTextFromValue (s.getValue()), r.toNearestInt(), juce::Justification::centred);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::accent.withAlpha (0.85f) : colours::panelHi);
    g.fillRoundedRectangle (r, 5.0f);
    if (highlighted)
    {
        g.setColour (colours::accent2.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }
    g.setColour (on ? juce::Colours::white : colours::dim);
    g.setFont (uiFont (11.5f, true));
    g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& pid, const juce::String& text, juce::Component* popupParent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, false, popupParent);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (uiFont (11.0f));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<APVTS::SliderAttachment> (s, pid, slider);
    if (auto* p = s.getParameter (pid))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromBottom (15));
    const int size = juce::jmin (b.getWidth(), b.getHeight(), 52);
    slider.setBounds (b.withSizeKeepingCentre (size, size));
}

Choice::Choice (APVTS& s, const juce::String& pid, const juce::String& text)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (pid)))
        box.addItemList (p->choices, 1);
    box.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (box);

    if (text.isNotEmpty())
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (uiFont (11.0f));
        addAndMakeVisible (label);
    }
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (s, pid, box);
}

void Choice::resized()
{
    auto b = getLocalBounds();
    if (label.isVisible()) label.setBounds (b.removeFromLeft (juce::jmin (60, b.getWidth() / 3)));
    box.setBounds (b);
}

Toggle::Toggle (APVTS& s, const juce::String& pid, const juce::String& text)
{
    button.setButtonText (text);
    addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (s, pid, button);
}

AmountBar::AmountBar (APVTS& s, const juce::String& pid, juce::Component* popupParent)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    juce::ignoreUnused (popupParent);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (s, pid, slider);
    slider.setDoubleClickReturnValue (true, 0.0);
}

//==============================================================================
WavetableView::WavetableView (APVTS& s, int oscIndex)
{
    table = s.getRawParameterValue (id::osc (oscIndex, "table"));
    pos = s.getRawParameterValue (id::osc (oscIndex, "pos"));
    on = s.getRawParameterValue (id::osc (oscIndex, "on"));
    startTimerHz (30);
}

void WavetableView::timerCallback()
{
    const float t = table->load(), p = pos->load(), o = on->load();
    if (t != lastTable || p != lastPos || o != lastOn)
    {
        lastTable = t; lastPos = p; lastOn = o;
        repaint();
    }
}

void WavetableView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (colours::bg);
    g.fillRoundedRectangle (r, 6.0f);

    const auto& wt = WavetableBank::get().table (juce::roundToInt (table->load()));
    const bool isOn = on->load() > 0.5f;
    const float position = pos->load();
    const auto area = r.reduced (8.0f);
    const float frameW = area.getWidth() * 0.74f, frameH = area.getHeight() * 0.5f;
    constexpr int points = 96;

    auto framePath = [&] (float k, auto sample)
    {
        const float rx = area.getX() + k * (area.getWidth() - frameW);
        const float ry = area.getBottom() - frameH - k * (area.getHeight() - frameH);
        juce::Path p;
        for (int i = 0; i < points; ++i)
        {
            const float v = sample ((int) ((float) i / (float) (points - 1) * (float) (kTableSize - 1)));
            const float px = rx + (float) i / (float) (points - 1) * frameW;
            const float py = ry + frameH * 0.5f * (1.0f - v * 0.9f);
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        return p;
    };

    const float alpha = isOn ? 1.0f : 0.3f;
    for (int f = kNumFrames - 1; f >= 0; --f)
    {
        const float* d = wt.get (f, 0);
        auto p = framePath ((float) f / (float) (kNumFrames - 1), [d] (int i) { return d[i]; });
        g.setColour (colours::dim.withAlpha (0.25f * alpha));
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    const float fpos = position * (float) (kNumFrames - 1);
    const int f0 = juce::jmin (kNumFrames - 2, (int) fpos);
    const float fr = fpos - (float) f0;
    const float* a = wt.get (f0, 0);
    const float* b = wt.get (f0 + 1, 0);
    auto current = framePath (position, [a, b, fr] (int i) { return a[i] + fr * (b[i] - a[i]); });
    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (alpha), area.getX(), 0.0f,
                                             colours::accent2.withAlpha (alpha), area.getRight(), 0.0f, false));
    g.strokePath (current, juce::PathStrokeType (2.0f));
}

} // namespace rum::ui

//==============================================================================
using namespace rum::ui;

RuminatorEditor::RuminatorEditor (RuminatorProcessor& p)
    : AudioProcessorEditor (p), proc (p)
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
    auto step = [this] (int delta)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + delta + n) % n);
        refreshPresetBox();
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (1); };

    topKnobs = { &knob ("master", "Master"), &knob ("glide", "Glide"), &knob ("bend_range", "Bend"), &knob ("vel_sens", "Velocity") };
    monoToggle = &make<Toggle> (s, "mono", "MONO");

    for (int o = 0; o < kNumOscs; ++o)
    {
        auto& ui = oscUI[(size_t) o];
        ui.on = &make<Toggle> (s, id::osc (o, "on"), "ON");
        ui.table = &make<Choice> (s, id::osc (o, "table"));
        ui.view = &make<WavetableView> (s, o);
        ui.knobs = { &knob (id::osc (o, "pos"), "Position"), &knob (id::osc (o, "warp"), "Warp"),
                     &knob (id::osc (o, "coarse"), "Coarse"), &knob (id::osc (o, "fine"), "Fine"),
                     &knob (id::osc (o, "level"), "Level"), &knob (id::osc (o, "pan"), "Pan"),
                     &knob (id::osc (o, "unison"), "Unison"), &knob (id::osc (o, "detune"), "Detune"),
                     &knob (id::osc (o, "spread"), "Spread") };
    }

    subShape = &make<Choice> (s, "sub_shape");
    subKnobs = { &knob ("sub_level", "Sub"), &knob ("noise_level", "Noise") };

    filterType = &make<Choice> (s, "flt_type");
    filterKnobs = { &knob ("flt_cutoff", "Cutoff"), &knob ("flt_res", "Reso"), &knob ("flt_drive", "Drive"),
                    &knob ("flt_env", "Env Amt"), &knob ("flt_key", "Keytrack") };

    ampKnobs = { &knob ("amp_a", "Attack"), &knob ("amp_d", "Decay"), &knob ("amp_s", "Sustain"), &knob ("amp_r", "Release") };
    modKnobs = { &knob ("mod_a", "Attack"), &knob ("mod_d", "Decay"), &knob ("mod_s", "Sustain"), &knob ("mod_r", "Release") };

    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& ui = lfoUI[(size_t) l];
        ui.shape = &make<Choice> (s, id::lfo (l, "shape"));
        ui.sync = &make<Toggle> (s, id::lfo (l, "sync"), "SYNC");
        ui.rate = &knob (id::lfo (l, "rate"), "Rate");
        ui.div = &make<Choice> (s, id::lfo (l, "div"));
    }

    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& ui = slotUI[(size_t) i];
        ui.src = &make<Choice> (s, id::slot (i, "src"));
        ui.dst = &make<Choice> (s, id::slot (i, "dst"));
        ui.amt = &make<AmountBar> (s, id::slot (i, "amt"), this);
    }

    for (int m = 0; m < kNumMacros; ++m)
        macroKnobs.push_back (&knob (id::macro (m), "Macro " + juce::String (m + 1)));

    fxUI[0].on = &make<Toggle> (s, "fx_dist_on", "DIST");
    fxUI[0].knobs = { &knob ("fx_dist_drive", "Drive"), &knob ("fx_dist_mix", "Mix") };
    fxUI[1].on = &make<Toggle> (s, "fx_cho_on", "CHORUS");
    fxUI[1].knobs = { &knob ("fx_cho_rate", "Rate"), &knob ("fx_cho_depth", "Depth"), &knob ("fx_cho_mix", "Mix") };
    fxUI[2].on = &make<Toggle> (s, "fx_dly_on", "DELAY");
    fxUI[2].choice = &make<Choice> (s, "fx_dly_div");
    fxUI[2].knobs = { &knob ("fx_dly_fb", "Feedback"), &knob ("fx_dly_mix", "Mix") };
    fxUI[3].on = &make<Toggle> (s, "fx_rev_on", "REVERB");
    fxUI[3].knobs = { &knob ("fx_rev_size", "Size"), &knob ("fx_rev_damp", "Damp"), &knob ("fx_rev_mix", "Mix") };

    setSize (1180, 800);
    startTimerHz (10);
}

RuminatorEditor::~RuminatorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RuminatorEditor::refreshPresetBox()
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

void RuminatorEditor::timerCallback()
{
    if (proc.getCurrentProgram() != shownProgram) refreshPresetBox();

    for (auto& ui : lfoUI)
    {
        const bool synced = ui.sync->button.getToggleState();
        ui.rate->setAlpha (synced ? 0.35f : 1.0f);
        ui.div->setAlpha (synced ? 1.0f : 0.35f);
    }
}

void RuminatorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    // Title
    g.setGradientFill (juce::ColourGradient (colours::accent, 16.0f, 0.0f, colours::accent2, 230.0f, 0.0f, false));
    g.setFont (uiFont (28.0f, true));
    g.drawText ("RUMINATOR", 16, 10, 240, 30, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (uiFont (11.0f));
    g.drawText ("wavetable synth  |  82 Jae", 18, 40, 240, 14, juce::Justification::centredLeft);

    for (auto& sec : sections)
    {
        const auto r = sec.area.toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (r.getX() + 10.0f, r.getY() + 9.0f, 3.0f, 11.0f, 1.5f);
        g.setColour (colours::text.withAlpha (0.85f));
        g.setFont (uiFont (12.0f, true));
        g.drawText (sec.title, sec.area.getX() + 18, sec.area.getY() + 5, sec.area.getWidth() - 24, 18,
                    juce::Justification::centredLeft);
    }
}

void RuminatorEditor::resized()
{
    sections.clear();
    auto area = getLocalBounds().reduced (10);
    constexpr int gap = 8;

    auto section = [this] (const juce::String& title, juce::Rectangle<int> r)
    {
        sections.push_back ({ title, r });
        return r.reduced (8).withTrimmedTop (20);
    };

    // Top bar
    auto top = area.removeFromTop (58);
    top.removeFromLeft (250);
    auto presetArea = top.removeFromLeft (300).withSizeKeepingCentre (300, 28);
    prevButton.setBounds (presetArea.removeFromLeft (28));
    nextButton.setBounds (presetArea.removeFromRight (28));
    presetBox.setBounds (presetArea.reduced (4, 0));
    top.removeFromLeft (20);
    monoToggle->setBounds (top.removeFromRight (70).withSizeKeepingCentre (64, 26));
    top.removeFromRight (10);
    for (auto it = topKnobs.rbegin(); it != topKnobs.rend(); ++it)
        (*it)->setBounds (top.removeFromRight (66));

    area.removeFromTop (gap);

    // Row 1: oscillators, sub/noise, filter
    auto row1 = area.removeFromTop (246);
    for (int o = 0; o < kNumOscs; ++o)
    {
        auto c = section ("OSC " + juce::String (o + 1), row1.removeFromLeft (372));
        row1.removeFromLeft (gap);
        auto& ui = oscUI[(size_t) o];
        auto head = c.removeFromTop (24);
        ui.on->setBounds (head.removeFromLeft (52));
        head.removeFromLeft (6);
        ui.table->setBounds (head);
        c.removeFromTop (6);
        ui.view->setBounds (c.removeFromLeft (112).reduced (0, 4));
        c.removeFromLeft (6);
        layoutGrid (c, comps (ui.knobs), 4);
    }

    {
        auto c = section ("SUB / NOISE", row1.removeFromLeft (118));
        row1.removeFromLeft (gap);
        subShape->setBounds (c.removeFromTop (24));
        c.removeFromTop (6);
        layoutGrid (c, comps (subKnobs), 1);
    }

    {
        auto c = section ("FILTER", row1);
        filterType->setBounds (c.removeFromTop (24));
        c.removeFromTop (6);
        layoutGrid (c, comps (filterKnobs), 3);
    }

    area.removeFromTop (gap);

    // Row 2: envelopes + LFOs
    auto row2 = area.removeFromTop (150);
    layoutGrid (section ("AMP ENV", row2.removeFromLeft (290)), comps (ampKnobs), 4);
    row2.removeFromLeft (gap);
    layoutGrid (section ("MOD ENV", row2.removeFromLeft (290)), comps (modKnobs), 4);
    row2.removeFromLeft (gap);
    const int lfoW = (row2.getWidth() - gap) / 2;
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto c = section ("LFO " + juce::String (l + 1), row2.removeFromLeft (lfoW));
        row2.removeFromLeft (gap);
        auto& ui = lfoUI[(size_t) l];
        auto head = c.removeFromTop (24);
        ui.sync->setBounds (head.removeFromRight (58));
        head.removeFromRight (6);
        ui.shape->setBounds (head);
        c.removeFromTop (4);
        ui.rate->setBounds (c.removeFromLeft (70));
        c.removeFromLeft (8);
        ui.div->setBounds (c.withSizeKeepingCentre (c.getWidth(), 24));
    }

    area.removeFromTop (gap);

    // Row 3: mod matrix, macros, FX
    auto row3 = area;
    {
        auto c = section ("MOD MATRIX", row3.removeFromLeft (440));
        row3.removeFromLeft (gap);
        const int rowH = c.getHeight() / kNumSlots;
        for (auto& ui : slotUI)
        {
            auto r = c.removeFromTop (rowH).withSizeKeepingCentre (c.getWidth(), juce::jmin (26, rowH - 4));
            ui.src->setBounds (r.removeFromLeft (120));
            r.removeFromLeft (6);
            ui.dst->setBounds (r.removeFromLeft (140));
            r.removeFromLeft (8);
            ui.amt->setBounds (r);
        }
    }
    {
        auto c = section ("MACROS", row3.removeFromLeft (150));
        row3.removeFromLeft (gap);
        layoutGrid (c, comps (macroKnobs), 2);
    }
    {
        auto c = section ("FX", row3);
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
            layoutGrid (col, comps (ui.knobs), ui.knobs.size() > 2 ? 2 : 1);
        }
    }
}
