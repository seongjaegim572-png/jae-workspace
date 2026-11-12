#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UiKit.h"

using namespace juce;

namespace
{
using jae::ui::Preset;

String pct (float v, int)  { return String (roundToInt (v * 100.0f)) + "%"; }
String bi (float v, int)   { auto p = roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; }
String sec (float v, int)  { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; }
String ms (float v, int)   { return String (roundToInt (v)) + " ms"; }
String hz (float v, int)   { return v < 1000.0f ? String (v, v < 10.0f ? 2 : 0) + " Hz" : String (v / 1000.0f, 1) + " kHz"; }
String perSec (float v, int) { return String (v, 1) + " /s"; }
String cents (float v, int)  { return String (roundToInt (v)) + " ct"; }
String db (float v, int)   { return String (v, 1) + " dB"; }

const StringArray delayDivs { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2" };
const double delayBeats[] = { 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
const StringArray arpDivs { "1/4", "1/8", "1/16", "1/8T", "1/16T" };
const double arpBeats[] = { 1.0, 0.5, 0.25, 1.0 / 3.0, 1.0 / 6.0 };

String lid (int layer, const char* name) { return String (layer == 0 ? "a_" : "b_") + name; }

// indices used by presets
enum { WT = 0, GRAN = 1 };
enum { Choir = 0, Air = 1, Bowed = 2, Shimmer = 3, Dusty = 4, User = 5 };
enum { Basic = 0, Pwm, Sync, Harmonic, Formant, Digital, Organ, Fold };

const std::vector<Preset>& presets()
{
    using namespace halo;
    static const std::vector<Preset> list {
        { "Init", {} },
        { "Glass Cathedral", {
            { "a_src", GRAN }, { "a_tex", Choir }, { "a_gpos", 0.3f }, { "a_spray", 0.3f }, { "a_gsize", 180.0f },
            { "a_density", 30.0f }, { "a_jitter", 0.1f }, { "a_cutoff", 9000.0f }, { "a_a", 0.8f }, { "a_d", 1.0f }, { "a_s", 0.9f }, { "a_r", 2.5f },
            { "b_on", 1 }, { "b_src", WT }, { "b_table", Harmonic }, { "b_pos", 0.4f }, { "b_detune", 0.3f }, { "b_coarse", 12 },
            { "b_level", 0.3f }, { "b_cutoff", 4000.0f }, { "b_a", 1.2f }, { "b_r", 3.0f },
            { "lfo1_rate", 0.1f }, { "mm1_src", src::Lfo1 }, { "mm1_dst", dst::APos }, { "mm1_amt", 0.25f },
            { "fx_cho_on", 1 }, { "fx_rev_size", 0.9f }, { "fx_rev_mix", 0.45f } } },
        { "Wind Pad", {
            { "a_src", GRAN }, { "a_tex", Air }, { "a_gsize", 300.0f }, { "a_density", 40.0f }, { "a_spray", 0.6f },
            { "a_cutoff", 3000.0f }, { "a_res", 0.3f }, { "a_a", 1.5f }, { "a_r", 3.0f },
            { "b_on", 1 }, { "b_src", WT }, { "b_table", Basic }, { "b_pos", 0.0f }, { "b_coarse", -12 }, { "b_level", 0.4f },
            { "b_a", 1.5f }, { "b_r", 3.0f },
            { "lfo1_rate", 0.07f }, { "mm1_src", src::Lfo1 }, { "mm1_dst", dst::ACutoff }, { "mm1_amt", 0.3f },
            { "fx_rev_size", 0.85f }, { "fx_rev_mix", 0.4f } } },
        { "Bowed Dream", {
            { "a_src", GRAN }, { "a_tex", Bowed }, { "a_gpos", 0.5f }, { "a_spray", 0.15f }, { "a_gsize", 120.0f }, { "a_density", 25.0f },
            { "a_a", 0.4f }, { "a_r", 1.5f },
            { "b_on", 1 }, { "b_src", GRAN }, { "b_tex", Bowed }, { "b_coarse", 12 }, { "b_level", 0.4f }, { "b_spray", 0.4f },
            { "b_jitter", 0.2f }, { "b_a", 0.8f }, { "b_r", 2.0f },
            { "lfo2_rate", 4.5f }, { "mm1_src", src::Lfo2 }, { "mm1_dst", dst::Pitch }, { "mm1_amt", 0.004f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 5 }, { "fx_dly_mix", 0.15f }, { "fx_rev_mix", 0.35f } } },
        { "Shimmer Keys", {
            { "a_src", GRAN }, { "a_tex", Shimmer }, { "a_gsize", 90.0f }, { "a_density", 40.0f }, { "a_spray", 0.2f },
            { "a_a", 0.005f }, { "a_d", 1.2f }, { "a_s", 0.3f }, { "a_r", 1.5f },
            { "b_on", 1 }, { "b_src", WT }, { "b_table", Formant }, { "b_pos", 0.3f }, { "b_level", 0.35f },
            { "b_a", 0.005f }, { "b_d", 0.8f }, { "b_s", 0.2f }, { "b_r", 1.0f },
            { "arp_on", 1 }, { "arp_mode", 0 }, { "arp_rate", 2 }, { "arp_oct", 2 }, { "arp_gate", 0.6f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 3 }, { "fx_dly_mix", 0.25f }, { "fx_rev_mix", 0.35f } } },
        { "Dusty Tape Pad", {
            { "a_src", GRAN }, { "a_tex", Dusty }, { "a_gsize", 200.0f }, { "a_density", 20.0f }, { "a_spray", 0.3f },
            { "a_a", 0.05f }, { "a_r", 1.0f },
            { "b_on", 1 }, { "b_src", WT }, { "b_table", Organ }, { "b_pos", 0.3f }, { "b_level", 0.45f }, { "b_cutoff", 2500.0f },
            { "b_a", 0.05f }, { "b_r", 1.0f },
            { "fx_dist_on", 1 }, { "fx_dist_drive", 0.2f }, { "fx_dist_mix", 0.3f }, { "fx_cho_on", 1 }, { "fx_rev_mix", 0.3f } } },
        { "Choir Arp", {
            { "a_src", GRAN }, { "a_tex", Choir }, { "a_gsize", 120.0f }, { "a_density", 35.0f },
            { "a_a", 0.01f }, { "a_d", 0.5f }, { "a_s", 0.6f }, { "a_r", 0.6f },
            { "arp_on", 1 }, { "arp_mode", 2 }, { "arp_rate", 1 }, { "arp_oct", 2 }, { "arp_gate", 0.8f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 5 }, { "fx_dly_mix", 0.2f }, { "fx_rev_mix", 0.4f } } },
        { "Glitch Texture", {
            { "a_src", GRAN }, { "a_tex", Shimmer }, { "a_jitter", 0.8f }, { "a_spray", 0.9f }, { "a_gsize", 40.0f }, { "a_density", 80.0f },
            { "b_on", 1 }, { "b_src", GRAN }, { "b_tex", Air }, { "b_level", 0.3f },
            { "lfo1_shape", 4 }, { "lfo1_rate", 6.0f }, { "mm1_src", src::Lfo1 }, { "mm1_dst", dst::GrainSize }, { "mm1_amt", 0.5f },
            { "mm2_src", src::Macro1 }, { "mm2_dst", dst::GrainSpray }, { "mm2_amt", 0.5f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 0 }, { "fx_dly_fb", 0.5f }, { "fx_dly_mix", 0.3f }, { "fx_rev_mix", 0.3f } } },
        { "Your Sample Pad", {
            { "a_src", GRAN }, { "a_tex", User }, { "a_gpos", 0.3f }, { "a_spray", 0.2f }, { "a_gsize", 200.0f }, { "a_density", 30.0f },
            { "a_a", 0.5f }, { "a_r", 2.0f }, { "fx_cho_on", 1 }, { "fx_rev_size", 0.8f }, { "fx_rev_mix", 0.4f } } },
    };
    return list;
}
}

AudioProcessorValueTreeState::ParameterLayout HaloProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto flt = [&l] (const String& pid, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> fn)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (fn)));
    };
    auto boolean = [&l] (const String& pid, const String& name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def)); };
    auto choice = [&l] (const String& pid, const String& name, const StringArray& items, int def)
    { l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, items, def)); };
    auto integer = [&l] (const String& pid, const String& name, int lo, int hi, int def)
    { l.add (std::make_unique<AudioParameterInt> (ParameterID { pid, 1 }, name, lo, hi, def)); };

    const NormalisableRange<float> unit (0.0f, 1.0f), bipolar (-1.0f, 1.0f);
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    for (int i = 0; i < halo::kLayers; ++i)
    {
        const String n = i == 0 ? "A " : "B ";
        boolean (lid (i, "on"), n + "On", i == 0);
        choice  (lid (i, "src"), n + "Source", { "Wavetable", "Granular" }, i == 0 ? 0 : 1);
        choice  (lid (i, "table"), n + "Wavetable", jae::tableNames(), 0);
        flt     (lid (i, "pos"), n + "WT Position", unit, 2.0f / 3.0f, pct);
        choice  (lid (i, "tex"), n + "Texture", halo::textureNames(), 0);
        flt     (lid (i, "gpos"), n + "Grain Position", unit, 0.3f, pct);
        flt     (lid (i, "spray"), n + "Spray", unit, 0.2f, pct);
        flt     (lid (i, "gsize"), n + "Grain Size", skewed (10.0f, 1000.0f, 150.0f), 150.0f, ms);
        flt     (lid (i, "density"), n + "Density", skewed (2.0f, 120.0f, 25.0f), 30.0f, perSec);
        flt     (lid (i, "jitter"), n + "Pitch Jitter", unit, 0.0f, pct);
        flt     (lid (i, "detune"), n + "Detune", unit, 0.2f, pct);
        integer (lid (i, "coarse"), n + "Coarse", -24, 24, 0);
        flt     (lid (i, "fine"), n + "Fine", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, cents);
        flt     (lid (i, "level"), n + "Level", unit, 0.8f, pct);
        flt     (lid (i, "pan"), n + "Pan", bipolar, 0.0f, bi);
        choice  (lid (i, "ftype"), n + "Filter", { "Low Pass", "Band Pass", "High Pass" }, 0);
        flt     (lid (i, "cutoff"), n + "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 8000.0f, hz);
        flt     (lid (i, "res"), n + "Resonance", unit, 0.1f, pct);
        flt     (lid (i, "a"), n + "Attack", skewed (0.001f, 10.0f, 0.6f), 0.01f, sec);
        flt     (lid (i, "d"), n + "Decay", skewed (0.001f, 10.0f, 0.6f), 0.3f, sec);
        flt     (lid (i, "s"), n + "Sustain", unit, 0.8f, pct);
        flt     (lid (i, "r"), n + "Release", skewed (0.001f, 10.0f, 0.6f), 0.5f, sec);
    }

    const StringArray shapes { "Sine", "Triangle", "Saw", "Square", "S&H" };
    for (int i = 0; i < 2; ++i)
    {
        const String id = "lfo" + String (i + 1) + "_";
        choice (id + "shape", "LFO" + String (i + 1) + " Shape", shapes, 0);
        flt (id + "rate", "LFO" + String (i + 1) + " Rate", skewed (0.02f, 20.0f, 1.0f), i == 0 ? 0.2f : 0.5f, hz);
    }
    flt ("menv_a", "Mod Attack", skewed (0.001f, 10.0f, 0.6f), 0.5f, sec);
    flt ("menv_d", "Mod Decay", skewed (0.001f, 10.0f, 0.6f), 1.0f, sec);
    flt ("menv_s", "Mod Sustain", unit, 0.5f, pct);
    flt ("menv_r", "Mod Release", skewed (0.001f, 10.0f, 0.6f), 1.0f, sec);

    for (int i = 0; i < halo::kSlots; ++i)
    {
        const String id = "mm" + String (i + 1) + "_", n = "Mod " + String (i + 1) + " ";
        choice (id + "src", n + "Source", halo::sourceNames(), i == 0 ? halo::src::ModWheel : 0);
        choice (id + "dst", n + "Destination", halo::destNames(), i == 0 ? halo::dst::ACutoff : 0);
        flt (id + "amt", n + "Amount", bipolar, i == 0 ? 0.3f : 0.0f, bi);
    }
    for (int i = 0; i < halo::kMacros; ++i)
        flt ("macro" + String (i + 1), "Macro " + String (i + 1), unit, 0.0f, pct);

    boolean ("arp_on", "Arp On", false);
    choice ("arp_mode", "Arp Mode", { "Up", "Down", "Up/Down", "Random", "As Played" }, 0);
    choice ("arp_rate", "Arp Rate", arpDivs, 2);
    integer ("arp_oct", "Arp Octaves", 1, 3, 1);
    flt ("arp_gate", "Arp Gate", NormalisableRange<float> (0.1f, 1.0f), 0.7f, pct);

    boolean ("fx_dist_on", "Dist On", false);
    flt ("fx_dist_drive", "Dist Drive", unit, 0.3f, pct);
    flt ("fx_dist_mix", "Dist Mix", unit, 0.4f, pct);
    boolean ("fx_cho_on", "Chorus On", false);
    flt ("fx_cho_rate", "Chorus Rate", skewed (0.05f, 5.0f, 1.0f), 0.5f, hz);
    flt ("fx_cho_depth", "Chorus Depth", unit, 0.4f, pct);
    flt ("fx_cho_mix", "Chorus Mix", unit, 0.35f, pct);
    boolean ("fx_dly_on", "Delay On", false);
    choice ("fx_dly_div", "Delay Time", delayDivs, 3);
    flt ("fx_dly_fb", "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.4f, pct);
    flt ("fx_dly_mix", "Delay Mix", unit, 0.2f, pct);
    boolean ("fx_rev_on", "Reverb On", true);
    flt ("fx_rev_size", "Reverb Size", unit, 0.75f, pct);
    flt ("fx_rev_damp", "Reverb Damping", unit, 0.4f, pct);
    flt ("fx_rev_mix", "Reverb Mix", unit, 0.25f, pct);

    integer ("user_root", "Sample Root Note", 24, 96, 60);
    flt ("vel_sens", "Velocity", unit, 0.5f, pct);
    flt ("master", "Master", NormalisableRange<float> (-48.0f, 6.0f), -6.0f, db);
    return l;
}

HaloProcessor::HaloProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HALO", createLayout())
{
    jae::WavetableBank::get();     // build tables/textures here, never on the audio thread
    halo::TextureBank::get();
    arpHeld.reserve (128);
    sustainedNotes.reserve (128);
}

bool HaloProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void HaloProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (size_t i = 0; i < voices.size(); ++i)
        voices[i].prepare (sampleRate, 0x85EBCA6Bu * (uint32_t) (i + 3));
    effects.prepare (sampleRate, samplesPerBlock);
    effects.reset();
    masterGain.reset (sampleRate, 0.03);
    masterGain.setCurrentAndTargetValue (Decibels::decibelsToGain (p ("master")));
    arpHeld.clear();
    arpPlaying = -1;
    samplesToOff = -1;
}

void HaloProcessor::readSettings()
{
    const auto* user = userSample.load();
    for (int i = 0; i < halo::kLayers; ++i)
    {
        auto& L = settings.layer[(size_t) i];
        L.on = p (lid (i, "on")) > 0.5f;
        L.source = roundToInt (p (lid (i, "src")));
        L.table = roundToInt (p (lid (i, "table")));
        L.pos = p (lid (i, "pos"));
        const int tex = roundToInt (p (lid (i, "tex")));
        if (tex == halo::kUserTexture && user != nullptr)
        {
            L.texture = user;
            L.root = p ("user_root");
        }
        else
        {
            const auto& t = halo::TextureBank::get().texture (tex == halo::kUserTexture ? 0 : tex);
            L.texture = &t;
            L.root = t.rootNote;
        }
        L.grainPos = p (lid (i, "gpos"));
        L.spray = p (lid (i, "spray"));
        L.grainMs = p (lid (i, "gsize"));
        L.density = p (lid (i, "density"));
        L.jitter = p (lid (i, "jitter"));
        L.detune = p (lid (i, "detune"));
        L.coarse = (float) roundToInt (p (lid (i, "coarse")));
        L.fine = p (lid (i, "fine"));
        L.level = p (lid (i, "level"));
        L.pan = p (lid (i, "pan"));
        L.filterType = roundToInt (p (lid (i, "ftype")));
        L.cutoff = p (lid (i, "cutoff"));
        L.res = p (lid (i, "res"));
        L.a = p (lid (i, "a")); L.d = p (lid (i, "d")); L.s = p (lid (i, "s")); L.r = p (lid (i, "r"));
    }
    for (int i = 0; i < 2; ++i)
    {
        settings.lfoShape[(size_t) i] = roundToInt (p ("lfo" + String (i + 1) + "_shape"));
        settings.lfoRate[(size_t) i] = p ("lfo" + String (i + 1) + "_rate");
    }
    settings.ma = p ("menv_a"); settings.md = p ("menv_d"); settings.ms = p ("menv_s"); settings.mr = p ("menv_r");
    for (int i = 0; i < halo::kSlots; ++i)
    {
        const String id = "mm" + String (i + 1) + "_";
        settings.slotSrc[(size_t) i] = roundToInt (p (id + "src"));
        settings.slotDst[(size_t) i] = roundToInt (p (id + "dst"));
        settings.slotAmt[(size_t) i] = p (id + "amt");
    }
    for (int i = 0; i < halo::kMacros; ++i) settings.macros[(size_t) i] = p ("macro" + String (i + 1));
    settings.velSens = p ("vel_sens");

    fx.distOn = p ("fx_dist_on") > 0.5f; fx.distDrive = p ("fx_dist_drive"); fx.distMix = p ("fx_dist_mix");
    fx.choOn = p ("fx_cho_on") > 0.5f; fx.choRate = p ("fx_cho_rate"); fx.choDepth = p ("fx_cho_depth"); fx.choMix = p ("fx_cho_mix");
    fx.dlyOn = p ("fx_dly_on") > 0.5f; fx.dlyFb = p ("fx_dly_fb"); fx.dlyMix = p ("fx_dly_mix");
    fx.dlyBeats = delayBeats[jlimit (0, 7, roundToInt (p ("fx_dly_div")))];
    fx.revOn = p ("fx_rev_on") > 0.5f; fx.revSize = p ("fx_rev_size"); fx.revDamp = p ("fx_rev_damp"); fx.revMix = p ("fx_rev_mix");
}

void HaloProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0) bpm = *b;

    readSettings();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    const bool arpOn = p ("arp_on") > 0.5f;
    if (arpWasOn && ! arpOn && arpPlaying >= 0) { voiceOff (arpPlaying); arpPlaying = -1; }
    arpWasOn = arpOn;
    const juce::int64 stepLen = jmax ((juce::int64) 1, (juce::int64) (arpBeats[jlimit (0, 4, roundToInt (p ("arp_rate")))] * 60.0 / bpm * sr));
    const float gate = p ("arp_gate");

    int pos = 0;
    auto it = midi.cbegin();
    while (pos < n)
    {
        const int nextMidi = it != midi.cend() ? jlimit (pos, n, (*it).samplePosition) : n;
        const bool arpRunning = arpOn && ! arpHeld.empty();
        int nextArp = n;
        if (arpRunning)
        {
            nextArp = (int) jmin ((juce::int64) n, pos + jmax ((juce::int64) 0, samplesToNext));
            if (samplesToOff >= 0) nextArp = (int) jmin ((juce::int64) nextArp, pos + samplesToOff);
        }
        const int t = jmin (nextMidi, nextArp);
        renderVoices (L, R, pos, t);

        if (arpRunning)
        {
            samplesToNext -= (t - pos);
            if (samplesToOff >= 0) samplesToOff -= (t - pos);
            if (samplesToOff == 0)
            {
                if (arpPlaying >= 0) voiceOff (arpPlaying);
                arpPlaying = -1;
                samplesToOff = -1;
            }
            if (samplesToNext <= 0)
            {
                arpStep();
                samplesToNext += stepLen;
                samplesToOff = jmax ((juce::int64) 1, (juce::int64) ((float) stepLen * gate));
            }
        }
        pos = t;

        if (it != midi.cend() && t == nextMidi)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }
    }
    midi.clear();

    effects.process (buffer, fx, bpm);

    masterGain.setTargetValue (Decibels::decibelsToGain (p ("master"), -48.0f));
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }
}

void HaloProcessor::renderVoices (float* L, float* R, int start, int end)
{
    if (end <= start) return;
    for (auto& v : voices) v.render (L + start, R + start, end - start, settings);
}

void HaloProcessor::voiceOn (int note, float vel)
{
    halo::Voice* chosen = nullptr;
    for (auto& v : voices)
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
    chosen->start (note, vel, ++counter, settings);
}

void HaloProcessor::voiceOff (int note)
{
    for (auto& v : voices)
        if (v.isActive() && v.isHeld() && v.getNote() == note) v.keyUp();
}

void HaloProcessor::arpStep()
{
    if (arpPlaying >= 0) { voiceOff (arpPlaying); arpPlaying = -1; }
    if (arpHeld.empty()) return;

    const int mode = roundToInt (p ("arp_mode"));
    const int octaves = jlimit (1, 3, roundToInt (p ("arp_oct")));

    std::array<int, 128> base {};
    int count = 0;
    for (auto& a : arpHeld) if (count < 128) base[(size_t) count++] = a.note;
    if (mode != 4) std::sort (base.begin(), base.begin() + count);

    const int len = count * octaves;
    auto at = [&] (int i) { return base[(size_t) (i % count)] + 12 * (i / count); };

    int idx;
    switch (mode)
    {
        case 1:  idx = len - 1 - (arpIndex % len); break;
        case 2:  { const int period = jmax (1, 2 * len - 2); const int k = arpIndex % period; idx = k < len ? k : period - k; break; }
        case 3:  idx = arpRandom.nextInt (len); break;
        default: idx = arpIndex % len; break;
    }
    ++arpIndex;
    const int note = jlimit (0, 127, at (idx));
    voiceOn (note, arpVel);
    arpPlaying = note;
}

void HaloProcessor::handleMidi (const MidiMessage& m)
{
    const bool arpOn = p ("arp_on") > 0.5f;
    if (m.isNoteOn())
    {
        if (arpOn)
        {
            if (arpHeld.empty()) { arpIndex = 0; samplesToNext = 0; }
            arpHeld.erase (std::remove_if (arpHeld.begin(), arpHeld.end(), [&] (const ArpNote& a) { return a.note == m.getNoteNumber(); }), arpHeld.end());
            arpHeld.push_back ({ m.getNoteNumber(), m.getFloatVelocity() });
            arpVel = m.getFloatVelocity();
        }
        else
        {
            voiceOn (m.getNoteNumber(), m.getFloatVelocity());
        }
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        arpHeld.erase (std::remove_if (arpHeld.begin(), arpHeld.end(), [note] (const ArpNote& a) { return a.note == note; }), arpHeld.end());
        if (arpOn)
        {
            if (arpHeld.empty() && arpPlaying >= 0) { voiceOff (arpPlaying); arpPlaying = -1; samplesToOff = -1; }
        }
        else if (sustainDown)
            sustainedNotes.push_back (note);
        else
            voiceOff (note);
    }
    else if (m.isSustainPedalOn())
        sustainDown = true;
    else if (m.isSustainPedalOff())
    {
        sustainDown = false;
        for (int note : sustainedNotes) voiceOff (note);
        sustainedNotes.clear();
    }
    else if (m.isController() && m.getControllerNumber() == 1)
        settings.modWheel = (float) m.getControllerValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        arpHeld.clear();
        arpPlaying = -1;
        sustainedNotes.clear();
        for (auto& v : voices) v.keyUp();
    }
}

//==============================================================================
bool HaloProcessor::loadUserSample (const File& file)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 64) return false;

    const int len = (int) jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0)); // max 60 s
    AudioBuffer<float> tmp ((int) jmax (1u, reader->numChannels), len);
    reader->read (&tmp, 0, len, 0, true, true);

    auto s = std::make_shared<halo::SampleData>();
    s->name = file.getFileNameWithoutExtension();
    s->sampleRate = reader->sampleRate;
    s->data.assign ((size_t) len, 0.0f);
    for (int c = 0; c < tmp.getNumChannels(); ++c)
    {
        const float* d = tmp.getReadPointer (c);
        for (int i = 0; i < len; ++i) s->data[(size_t) i] += d[i] / (float) tmp.getNumChannels();
    }
    float peak = 1.0e-6f;
    for (auto v : s->data) peak = jmax (peak, std::abs (v));
    for (auto& v : s->data) v *= 0.9f / peak;

    loadedSamples.push_back (s);
    userSample.store (s.get());
    userSampleFile = file;
    if (loadedSamples.size() > 4) loadedSamples.erase (loadedSamples.begin()); // old ones are no longer read
    return true;
}

String HaloProcessor::getUserSampleName() const
{
    const auto* s = userSample.load();
    return s != nullptr ? s->name : String ("No sample loaded");
}

int HaloProcessor::getNumPrograms() { return (int) presets().size(); }

const String HaloProcessor::getProgramName (int index)
{
    return isPositiveAndBelow (index, getNumPrograms()) ? String (presets()[(size_t) index].name) : String();
}

void HaloProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, getNumPrograms()) || index == currentProgram) return;
    currentProgram = index;
    jae::ui::applyPreset (apvts, presets()[(size_t) index]);
}

void HaloProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("userSample", userSampleFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void HaloProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            const String path = tree.getProperty ("userSample", "").toString();
            apvts.replaceState (tree);
            if (path.isNotEmpty() && File::isAbsolutePath (path) && File (path).existsAsFile())
                loadUserSample (File (path));
        }
}

AudioProcessorEditor* HaloProcessor::createEditor() { return new HaloEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HaloProcessor(); }
