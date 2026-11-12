#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UiKit.h"

using namespace juce;

namespace
{
using jae::ui::Preset;

String pct (float v, int)  { return String (roundToInt (v * 100.0f)) + "%"; }
String sec (float v, int)  { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; }
String hz (float v, int)   { return String (v, 2) + " Hz"; }
String db (float v, int)   { return String (v, 1) + " dB"; }
String mult (float v, int) { return String (v, 2) + "x"; }

const StringArray delayDivs { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2" };
const double delayBeats[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };

const std::vector<Preset>& presets()
{
    using namespace keys::model;
    static const std::vector<Preset> list {
        { "Init", {} },
        { "Concert Grand", { { "model", Grand }, { "tone", 0.6f }, { "hardness", 0.6f }, { "fx_rev_size", 0.7f }, { "fx_rev_mix", 0.22f } } },
        { "Felt Grand", { { "model", Grand }, { "tone", 0.15f }, { "hardness", 0.2f }, { "noise", 0.55f }, { "release", 0.4f },
                          { "fx_rev_size", 0.75f }, { "fx_rev_mix", 0.3f } } },
        { "Lofi Grand", { { "model", Grand }, { "tone", 0.35f }, { "wear", 0.5f }, { "fx_cho_on", 1 }, { "fx_cho_mix", 0.25f },
                          { "fx_cho_rate", 0.3f }, { "fx_rev_mix", 0.25f } } },
        { "Tine EP", { { "model", TineEP }, { "tone", 0.5f }, { "hardness", 0.55f }, { "drive", 0.25f }, { "fx_rev_mix", 0.15f } } },
        { "Tine EP Trem", { { "model", TineEP }, { "hardness", 0.5f }, { "trem_depth", 0.6f }, { "trem_rate", 4.5f },
                            { "fx_phs_on", 1 }, { "fx_phs_mix", 0.3f }, { "fx_rev_mix", 0.2f } } },
        { "Reed EP Bark", { { "model", ReedEP }, { "drive", 0.5f }, { "hardness", 0.7f }, { "trem_depth", 0.2f }, { "fx_rev_mix", 0.15f } } },
        { "Funk Clav", { { "model", Clav }, { "tone", 0.7f }, { "hardness", 0.7f }, { "decay", 0.8f }, { "release", 0.08f },
                         { "fx_phs_on", 1 }, { "fx_phs_mix", 0.4f }, { "fx_rev_on", 0 } } },
        { "Vibes", { { "model", Vibes }, { "trem_depth", 0.35f }, { "trem_rate", 5.5f }, { "release", 0.6f }, { "fx_rev_mix", 0.3f } } },
        { "Music Box Dream", { { "model", MusicBox }, { "tone", 0.6f }, { "fx_dly_on", 1 }, { "fx_dly_div", 3 }, { "fx_dly_mix", 0.2f },
                               { "fx_rev_size", 0.85f }, { "fx_rev_mix", 0.4f } } },
        { "Celesta", { { "model", Celesta }, { "release", 0.5f }, { "fx_rev_mix", 0.3f } } },
    };
    return list;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout KeysProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto flt = [&l] (const char* pid, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> fn)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (fn)));
    };
    auto boolean = [&l] (const char* pid, const char* name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def)); };
    auto choice = [&l] (const char* pid, const char* name, const StringArray& items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, items, def)); };

    const NormalisableRange<float> unit (0.0f, 1.0f);
    NormalisableRange<float> decay (0.25f, 2.0f); decay.setSkewForCentre (1.0f);
    NormalisableRange<float> rel (0.02f, 3.0f);   rel.setSkewForCentre (0.4f);
    NormalisableRange<float> trem (0.5f, 10.0f);  trem.setSkewForCentre (4.0f);
    NormalisableRange<float> lfo (0.05f, 5.0f);   lfo.setSkewForCentre (1.0f);

    choice ("model", "Instrument", keys::modelNames(), keys::model::Grand);
    flt ("tone", "Tone", unit, 0.5f, pct);
    flt ("hardness", "Hardness", unit, 0.5f, pct);
    flt ("decay", "Decay", decay, 1.0f, mult);
    flt ("release", "Release", rel, 0.25f, sec);
    flt ("noise", "Mechanics", unit, 0.3f, pct);
    flt ("wear", "Wear", unit, 0.0f, pct);
    flt ("drive", "Drive", unit, 0.2f, pct);
    flt ("width", "Stereo Width", unit, 0.6f, pct);
    flt ("vel_sens", "Velocity", unit, 0.7f, pct);
    flt ("trem_rate", "Tremolo Rate", trem, 4.5f, hz);
    flt ("trem_depth", "Tremolo Depth", unit, 0.0f, pct);
    flt ("master", "Master", NormalisableRange<float> (-48.0f, 6.0f), -6.0f, db);

    boolean ("fx_phs_on", "Phaser On", false);
    flt ("fx_phs_rate", "Phaser Rate", lfo, 0.4f, hz);
    flt ("fx_phs_depth", "Phaser Depth", unit, 0.6f, pct);
    flt ("fx_phs_mix", "Phaser Mix", unit, 0.5f, pct);
    boolean ("fx_cho_on", "Chorus On", false);
    flt ("fx_cho_rate", "Chorus Rate", lfo, 0.8f, hz);
    flt ("fx_cho_depth", "Chorus Depth", unit, 0.4f, pct);
    flt ("fx_cho_mix", "Chorus Mix", unit, 0.4f, pct);
    boolean ("fx_dly_on", "Delay On", false);
    choice ("fx_dly_div", "Delay Time", delayDivs, 3);
    flt ("fx_dly_fb", "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.35f, pct);
    flt ("fx_dly_mix", "Delay Mix", unit, 0.2f, pct);
    boolean ("fx_rev_on", "Reverb On", true);
    flt ("fx_rev_size", "Reverb Size", unit, 0.6f, pct);
    flt ("fx_rev_damp", "Reverb Damping", unit, 0.5f, pct);
    flt ("fx_rev_mix", "Reverb Mix", unit, 0.2f, pct);
    return l;
}

KeysProcessor::KeysProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KEYS82", createLayout())
{
}

bool KeysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void KeysProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (size_t i = 0; i < voices.size(); ++i)
        voices[i].prepare (sampleRate, 0x9E3779B9u * (uint32_t) (i + 7));
    effects.prepare (sampleRate, samplesPerBlock);
    effects.reset();
    masterGain.reset (sampleRate, 0.03);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("master")));
    sustainDown = false;
}

void KeysProcessor::readSettings()
{
    settings.model = juce::roundToInt (p ("model"));
    settings.tone = p ("tone");
    settings.hardness = p ("hardness");
    settings.decay = p ("decay");
    settings.release = p ("release");
    settings.noise = p ("noise");
    settings.wear = p ("wear");
    settings.drive = p ("drive");
    settings.width = p ("width");
    settings.velSens = p ("vel_sens");

    fx.distOn = false;
    fx.phsOn = p ("fx_phs_on") > 0.5f; fx.phsRate = p ("fx_phs_rate"); fx.phsDepth = p ("fx_phs_depth"); fx.phsMix = p ("fx_phs_mix");
    fx.choOn = p ("fx_cho_on") > 0.5f; fx.choRate = p ("fx_cho_rate"); fx.choDepth = p ("fx_cho_depth"); fx.choMix = p ("fx_cho_mix");
    fx.dlyOn = p ("fx_dly_on") > 0.5f; fx.dlyFb = p ("fx_dly_fb"); fx.dlyMix = p ("fx_dly_mix");
    fx.dlyBeats = delayBeats[juce::jlimit (0, 7, juce::roundToInt (p ("fx_dly_div")))];
    fx.revOn = p ("fx_rev_on") > 0.5f; fx.revSize = p ("fx_rev_size"); fx.revDamp = p ("fx_rev_damp"); fx.revMix = p ("fx_rev_mix");
}

void KeysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0) bpm = *b;

    readSettings();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, n, meta.samplePosition);
        renderVoices (L, R, pos, t);
        handleMidi (meta.getMessage());
        pos = t;
    }
    renderVoices (L, R, pos, n);
    midi.clear();

    // tremolo / auto-pan (classic EP amp)
    const float depth = p ("trem_depth");
    const float inc = p ("trem_rate") / (float) sr;
    for (int i = 0; i < n; ++i)
    {
        if (depth > 0.0f)
        {
            const float s = std::sin (juce::MathConstants<float>::twoPi * tremPhase);
            L[i] *= 1.0f - depth * 0.5f * (1.0f + s);
            R[i] *= 1.0f - depth * 0.5f * (1.0f - s);
        }
        tremPhase += inc;
        if (tremPhase >= 1.0f) tremPhase -= 1.0f;
    }

    effects.process (buffer, fx, bpm);

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (p ("master"), -48.0f));
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }
}

void KeysProcessor::renderVoices (float* L, float* R, int start, int end)
{
    if (end <= start) return;
    for (auto& v : voices) v.render (L + start, R + start, end - start, settings);
}

void KeysProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        keys::Voice* chosen = nullptr;
        for (auto& v : voices)   // re-striking a held/sustained key reuses its voice
            if (v.isActive() && v.getNote() == note) { chosen = &v; break; }
        if (chosen == nullptr)
            for (auto& v : voices)
                if (! v.isActive()) { chosen = &v; break; }
        if (chosen == nullptr)
            for (auto& v : voices)
                if (v.isReleasing() && (chosen == nullptr || v.getAge() < chosen->getAge())) chosen = &v;
        if (chosen == nullptr)
            for (auto& v : voices)
                if (chosen == nullptr || v.getAge() < chosen->getAge()) chosen = &v;
        chosen->start (note, m.getFloatVelocity(), ++counter, settings);
    }
    else if (m.isNoteOff())
    {
        for (auto& v : voices)
            if (v.isActive() && v.isHeld() && v.getNote() == m.getNoteNumber())
                v.keyUp (sustainDown, settings);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) v.keyUp (false, settings);
    }
    else if (m.isSustainPedalOn())
    {
        sustainDown = true;
    }
    else if (m.isSustainPedalOff())
    {
        sustainDown = false;
        for (auto& v : voices) v.pedalUp (settings);
    }
}

int KeysProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String KeysProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (presets()[(size_t) index].name) : juce::String();
}

void KeysProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()) || index == currentProgram) return;
    currentProgram = index;
    jae::ui::applyPreset (apvts, presets()[(size_t) index]);
}

void KeysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void KeysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* KeysProcessor::createEditor() { return new KeysEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KeysProcessor(); }
