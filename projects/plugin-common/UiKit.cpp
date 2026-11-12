#include "UiKit.h"

namespace jae::ui
{
juce::Font uiFont (float size, bool bold)
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

void paintSections (juce::Graphics& g, const std::vector<Section>& sections, juce::Colour accent)
{
    for (auto& sec : sections)
    {
        const auto r = sec.area.toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (r.getX() + 10.0f, r.getY() + 9.0f, 3.0f, 11.0f, 1.5f);
        g.setColour (colours::text.withAlpha (0.85f));
        g.setFont (uiFont (12.0f, true));
        g.drawText (sec.title, sec.area.getX() + 18, sec.area.getY() + 5, sec.area.getWidth() - 24, 18,
                    juce::Justification::centredLeft);
    }
}

void paintTitle (juce::Graphics& g, const juce::String& title, const juce::String& subtitle,
                 juce::Colour a, juce::Colour b)
{
    g.setGradientFill (juce::ColourGradient (a, 16.0f, 0.0f, b, 230.0f, 0.0f, false));
    g.setFont (uiFont (28.0f, true));
    g.drawText (title, 16, 10, 260, 30, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (uiFont (11.0f));
    g.drawText (subtitle, 18, 40, 260, 14, juce::Justification::centredLeft);
}

//==============================================================================
LookAndFeel::LookAndFeel (juce::Colour a, juce::Colour b) : acc (a), acc2 (b)
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg);
    setColour (juce::Label::textColourId, colours::dim);
    setColour (juce::ComboBox::backgroundColourId, colours::panelHi);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, acc);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, colours::panelHi);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::BubbleComponent::backgroundColourId, colours::panelHi);
    setColour (juce::BubbleComponent::outlineColourId, acc);
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
        g.setGradientFill (juce::ColourGradient (acc, cx - radius, cy + radius, acc2, cx + radius, cy - radius, false));
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
    g.setColour (s.getValue() >= 0.0 ? acc : acc2);
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
    g.setColour (on ? acc.withAlpha (0.85f) : colours::panelHi);
    g.fillRoundedRectangle (r, 5.0f);
    if (highlighted)
    {
        g.setColour (acc2.withAlpha (0.6f));
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

AmountBar::AmountBar (APVTS& s, const juce::String& pid, juce::Component*)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (s, pid, slider);
    slider.setDoubleClickReturnValue (true, 0.0);
}
} // namespace jae::ui
