#include "PluginEditor.h"

using namespace jae::ui;

namespace
{
juce::String lid (int layer, const char* name) { return juce::String (layer == 0 ? "a_" : "b_") + name; }
}

//==============================================================================
LayerView::LayerView (HaloProcessor& p, int layerIndex) : proc (p), layer (layerIndex)
{
    startTimerHz (20);
}

void LayerView::timerCallback()
{
    auto v = [this] (const char* n) { return juce::String (proc.apvts.getRawParameterValue (lid (layer, n))->load(), 3); };
    const auto key = v ("on") + v ("src") + v ("table") + v ("pos") + v ("tex") + v ("gpos") + v ("spray") + proc.getUserSampleName();
    if (key != lastKey) { lastKey = key; repaint(); }
}

void LayerView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (colours::bg);
    g.fillRoundedRectangle (r, 6.0f);

    auto val = [this] (const char* n) { return proc.apvts.getRawParameterValue (lid (layer, n))->load(); };
    const bool on = val ("on") > 0.5f;
    const bool granular = juce::roundToInt (val ("src")) == 1;
    const float alpha = on ? 1.0f : 0.3f;
    const auto inner = r.reduced (6.0f, 4.0f);
    const float mid = inner.getCentreY(), half = inner.getHeight() * 0.45f;
    auto* lf = dynamic_cast<LookAndFeel*> (&getLookAndFeel());
    const auto acc = lf != nullptr ? lf->acc : colours::accent;
    const auto acc2 = lf != nullptr ? lf->acc2 : colours::accent2;

    if (granular)
    {
        const int tex = juce::roundToInt (val ("tex"));
        const auto& builtIn = halo::TextureBank::get().texture (tex == halo::kUserTexture ? 0 : tex);
        const std::vector<float>* data = &builtIn.data;
        juce::String name = builtIn.name;
        if (tex == halo::kUserTexture)
            name = proc.getUserSampleName() + (proc.getUserSampleName() == "No sample loaded" ? " (using Glass Choir)" : "");

        // grain position + spray band
        const float gp = val ("gpos"), spray = val ("spray");
        const float x = inner.getX() + gp * inner.getWidth();
        const float w = spray * 0.5f * inner.getWidth();
        g.setColour (acc2.withAlpha (0.15f * alpha));
        g.fillRect (juce::Rectangle<float> (x - w, inner.getY(), 2.0f * w, inner.getHeight()));

        const int cols = juce::jmax (1, (int) inner.getWidth());
        const size_t step = juce::jmax ((size_t) 1, data->size() / (size_t) cols);
        g.setColour (acc.withAlpha (0.8f * alpha));
        for (int c = 0; c < cols; ++c)
        {
            float pk = 0.0f;
            const size_t start = (size_t) c * step;
            for (size_t i = start; i < juce::jmin (data->size(), start + step); i += 4) pk = juce::jmax (pk, std::abs ((*data)[i]));
            g.drawVerticalLine ((int) inner.getX() + c, mid - pk * half, mid + pk * half);
        }
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.drawVerticalLine ((int) x, inner.getY(), inner.getBottom());
        g.setColour (colours::text.withAlpha (0.7f * alpha));
        g.setFont (uiFont (11.0f));
        g.drawText (name, inner.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::topLeft);
    }
    else
    {
        const auto& wt = jae::WavetableBank::get().table (juce::roundToInt (val ("table")));
        const float position = val ("pos") * (float) (jae::kNumFrames - 1);
        const int f0 = juce::jmin (jae::kNumFrames - 2, (int) position);
        const float fr = position - (float) f0;
        const float* a = wt.get (f0, 0);
        const float* b = wt.get (f0 + 1, 0);
        juce::Path path;
        constexpr int pts = 160;
        for (int i = 0; i < pts; ++i)
        {
            const int idx = (int) ((float) i / (float) (pts - 1) * (float) (jae::kTableSize - 1));
            const float v = a[idx] + fr * (b[idx] - a[idx]);
            const float px = inner.getX() + (float) i / (float) (pts - 1) * inner.getWidth();
            if (i == 0) path.startNewSubPath (px, mid - v * half); else path.lineTo (px, mid - v * half);
        }
        g.setGradientFill (juce::ColourGradient (acc.withAlpha (alpha), inner.getX(), 0.0f, acc2.withAlpha (alpha), inner.getRight(), 0.0f, false));
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }
}

//==============================================================================
HaloEditor::HaloEditor (HaloProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (sampleLabel);
    sampleLabel.setFont (uiFont (11.0f));
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
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a sample or loop", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile()) loadFile (fc.getResult());
                              });
    };

    topKnobs = { &knob ("user_root", "Root"), &knob ("vel_sens", "Velocity"), &knob ("master", "Master") };

    for (int i = 0; i < halo::kLayers; ++i)
    {
        auto& ui = layerUI[(size_t) i];
        ui.on = &bin.make<Toggle> (*this, s, lid (i, "on"), i == 0 ? "A" : "B");
        ui.source = &bin.make<Choice> (*this, s, lid (i, "src"));
        ui.table = &bin.make<Choice> (*this, s, lid (i, "table"));
        ui.texture = &bin.make<Choice> (*this, s, lid (i, "tex"));
        ui.filter = &bin.make<Choice> (*this, s, lid (i, "ftype"));
        ui.view = &bin.make<LayerView> (*this, proc, i);
        auto* pos = &knob (lid (i, "pos"), "WT Pos");
        auto* gpos = &knob (lid (i, "gpos"), "Grain Pos");
        auto* spray = &knob (lid (i, "spray"), "Spray");
        auto* size = &knob (lid (i, "gsize"), "Size");
        auto* dens = &knob (lid (i, "density"), "Density");
        auto* jit = &knob (lid (i, "jitter"), "Jitter");
        auto* det = &knob (lid (i, "detune"), "Detune");
        ui.knobs = { pos, gpos, spray, size, dens, jit, det,
                     &knob (lid (i, "coarse"), "Coarse"), &knob (lid (i, "fine"), "Fine"),
                     &knob (lid (i, "level"), "Level"), &knob (lid (i, "pan"), "Pan"),
                     &knob (lid (i, "cutoff"), "Cutoff"), &knob (lid (i, "res"), "Reso"),
                     &knob (lid (i, "a"), "Attack"), &knob (lid (i, "d"), "Decay"),
                     &knob (lid (i, "s"), "Sustain"), &knob (lid (i, "r"), "Release") };
        ui.wtOnly = { pos, det };
        ui.granOnly = { gpos, spray, size, dens, jit };
    }

    for (int i = 0; i < 2; ++i)
    {
        const juce::String id = "lfo" + juce::String (i + 1) + "_";
        lfoUI[(size_t) i].shape = &bin.make<Choice> (*this, s, id + "shape");
        lfoUI[(size_t) i].rate = &knob (id + "rate", "Rate");
    }
    modEnvKnobs = { &knob ("menv_a", "Attack"), &knob ("menv_d", "Decay"), &knob ("menv_s", "Sustain"), &knob ("menv_r", "Release") };

    arpOn = &bin.make<Toggle> (*this, s, "arp_on", "ARP");
    arpMode = &bin.make<Choice> (*this, s, "arp_mode");
    arpRate = &bin.make<Choice> (*this, s, "arp_rate");
    arpKnobs = { &knob ("arp_oct", "Octaves"), &knob ("arp_gate", "Gate") };

    for (int i = 0; i < halo::kSlots; ++i)
    {
        const juce::String id = "mm" + juce::String (i + 1) + "_";
        auto& ui = slotUI[(size_t) i];
        ui.src = &bin.make<Choice> (*this, s, id + "src");
        ui.dst = &bin.make<Choice> (*this, s, id + "dst");
        ui.amt = &bin.make<AmountBar> (*this, s, id + "amt", this);
    }
    for (int i = 0; i < halo::kMacros; ++i)
        macroKnobs.push_back (&knob ("macro" + juce::String (i + 1), "Macro " + juce::String (i + 1)));

    fxUI[0].on = &bin.make<Toggle> (*this, s, "fx_dist_on", "DIST");
    fxUI[0].knobs = { &knob ("fx_dist_drive", "Drive"), &knob ("fx_dist_mix", "Mix") };
    fxUI[1].on = &bin.make<Toggle> (*this, s, "fx_cho_on", "CHORUS");
    fxUI[1].knobs = { &knob ("fx_cho_rate", "Rate"), &knob ("fx_cho_depth", "Depth"), &knob ("fx_cho_mix", "Mix") };
    fxUI[2].on = &bin.make<Toggle> (*this, s, "fx_dly_on", "DELAY");
    fxUI[2].choice = &bin.make<Choice> (*this, s, "fx_dly_div");
    fxUI[2].knobs = { &knob ("fx_dly_fb", "Feedback"), &knob ("fx_dly_mix", "Mix") };
    fxUI[3].on = &bin.make<Toggle> (*this, s, "fx_rev_on", "REVERB");
    fxUI[3].knobs = { &knob ("fx_rev_size", "Size"), &knob ("fx_rev_damp", "Damp"), &knob ("fx_rev_mix", "Mix") };

    setSize (1200, 840);
    startTimerHz (10);
    timerCallback();
}

HaloEditor::~HaloEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

Knob& HaloEditor::knob (const juce::String& pid, const juce::String& text)
{
    return bin.make<Knob> (*this, proc.apvts, pid, text, this);
}

void HaloEditor::refreshPresetBox()
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

void HaloEditor::loadFile (const juce::File& f)
{
    if (proc.loadUserSample (f))
    {
        // point layer A at the user sample in granular mode, so it is heard right away
        auto set = [this] (const juce::String& pid, float v)
        {
            if (auto* p = proc.apvts.getParameter (pid)) p->setValueNotifyingHost (p->convertTo0to1 (v));
        };
        set ("a_src", 1.0f);
        set ("a_tex", (float) halo::kUserTexture);
    }
    timerCallback();
}

bool HaloEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff")
            || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".mp3"))
            return true;
    return false;
}

void HaloEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty()) loadFile (juce::File (files[0]));
}

void HaloEditor::timerCallback()
{
    if (proc.getCurrentProgram() != shownProgram) refreshPresetBox();
    sampleLabel.setText ("Sample: " + proc.getUserSampleName() + "   (drag a WAV here)", juce::dontSendNotification);

    for (int i = 0; i < halo::kLayers; ++i)
    {
        auto& ui = layerUI[(size_t) i];
        const bool gran = juce::roundToInt (proc.apvts.getRawParameterValue (lid (i, "src"))->load()) == 1;
        for (auto* k : ui.wtOnly) k->setAlpha (gran ? 0.3f : 1.0f);
        for (auto* k : ui.granOnly) k->setAlpha (gran ? 1.0f : 0.3f);
        ui.table->setAlpha (gran ? 0.3f : 1.0f);
        ui.texture->setAlpha (gran ? 1.0f : 0.3f);
    }
}

void HaloEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    paintTitle (g, "HALO", "granular + wavetable  |  82 Jae", lnf.acc, lnf.acc2);
    paintSections (g, sections, lnf.acc);
}

void HaloEditor::resized()
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
    top.removeFromLeft (200);
    auto presetArea = top.removeFromLeft (300).withSizeKeepingCentre (300, 28);
    prevButton.setBounds (presetArea.removeFromLeft (28));
    nextButton.setBounds (presetArea.removeFromRight (28));
    presetBox.setBounds (presetArea.reduced (4, 0));
    top.removeFromLeft (16);
    for (auto it = topKnobs.rbegin(); it != topKnobs.rend(); ++it) (*it)->setBounds (top.removeFromRight (66));
    auto sampleArea = top.reduced (0, 8);
    loadButton.setBounds (sampleArea.removeFromLeft (110));
    sampleArea.removeFromLeft (8);
    sampleLabel.setBounds (sampleArea);

    area.removeFromTop (gap);
    auto row1 = area.removeFromTop (330);
    const int layerW = (row1.getWidth() - gap) / 2;
    for (int i = 0; i < halo::kLayers; ++i)
    {
        auto c = section (i == 0 ? "LAYER A" : "LAYER B", row1.removeFromLeft (layerW));
        row1.removeFromLeft (gap);
        auto& ui = layerUI[(size_t) i];
        auto head = c.removeFromTop (26);
        ui.on->setBounds (head.removeFromLeft (40));
        head.removeFromLeft (6);
        const int w = (head.getWidth() - 18) / 4;
        ui.source->setBounds (head.removeFromLeft (w)); head.removeFromLeft (6);
        ui.table->setBounds (head.removeFromLeft (w));  head.removeFromLeft (6);
        ui.texture->setBounds (head.removeFromLeft (w)); head.removeFromLeft (6);
        ui.filter->setBounds (head);
        c.removeFromTop (6);
        ui.view->setBounds (c.removeFromTop (70));
        c.removeFromTop (4);
        layoutGrid (c, comps (ui.knobs), 9);
    }

    area.removeFromTop (gap);
    auto row2 = area.removeFromTop (140);
    for (int i = 0; i < 2; ++i)
    {
        auto c = section ("LFO " + juce::String (i + 1), row2.removeFromLeft (190));
        row2.removeFromLeft (gap);
        lfoUI[(size_t) i].shape->setBounds (c.removeFromTop (24));
        lfoUI[(size_t) i].rate->setBounds (c.withSizeKeepingCentre (70, c.getHeight()));
    }
    layoutGrid (section ("MOD ENV", row2.removeFromLeft (300)), comps (modEnvKnobs), 4);
    row2.removeFromLeft (gap);
    {
        auto c = section ("ARPEGGIATOR", row2);
        auto left = c.removeFromLeft (200);
        arpOn->setBounds (left.removeFromTop (24));
        left.removeFromTop (6);
        arpMode->setBounds (left.removeFromTop (24));
        left.removeFromTop (6);
        arpRate->setBounds (left.removeFromTop (24));
        c.removeFromLeft (10);
        layoutGrid (c, comps (arpKnobs), 2);
    }

    area.removeFromTop (gap);
    auto row3 = area;
    {
        auto c = section ("MOD MATRIX", row3.removeFromLeft (420));
        row3.removeFromLeft (gap);
        const int rowH = c.getHeight() / halo::kSlots;
        for (auto& ui : slotUI)
        {
            auto r = c.removeFromTop (rowH).withSizeKeepingCentre (c.getWidth(), juce::jmin (26, rowH - 4));
            ui.src->setBounds (r.removeFromLeft (115));
            r.removeFromLeft (6);
            ui.dst->setBounds (r.removeFromLeft (135));
            r.removeFromLeft (8);
            ui.amt->setBounds (r);
        }
    }
    layoutGrid (section ("MACROS", row3.removeFromLeft (150)), comps (macroKnobs), 2);
    row3.removeFromLeft (gap);
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
