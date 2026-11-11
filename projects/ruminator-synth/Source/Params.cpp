#include "Params.h"
#include "Wavetables.h"

namespace rum
{
using namespace juce;

juce::StringArray modSourceNames()
{
    return { "None", "LFO 1", "LFO 2", "Mod Env", "Amp Env", "Velocity", "Mod Wheel",
             "Aftertouch", "Keytrack", "Random", "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
}

juce::StringArray modDestNames()
{
    return { "None", "Osc1 Position", "Osc2 Position", "Osc1 Pitch", "Osc2 Pitch", "Osc1+2 Pitch",
             "Osc1 Warp", "Osc2 Warp", "Osc1 Level", "Osc2 Level", "Osc1 Detune", "Osc2 Detune",
             "Cutoff", "Resonance", "Drive", "Amp", "Pan", "LFO1 Rate", "LFO2 Rate" };
}

juce::StringArray filterTypeNames() { return { "LP 12", "LP 24", "Band", "High", "Notch" }; }
juce::StringArray lfoShapeNames()   { return { "Sine", "Triangle", "Saw", "Square", "S&H" }; }

const std::vector<Division>& lfoDivisions()
{
    static const std::vector<Division> d {
        { "4 bars", 16.0 }, { "2 bars", 8.0 }, { "1 bar", 4.0 }, { "1/2", 2.0 }, { "1/4D", 1.5 },
        { "1/4", 1.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8D", 0.75 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 },
        { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 }
    };
    return d;
}

const std::vector<Division>& delayDivisions()
{
    static const std::vector<Division> d {
        { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/16D", 0.375 },
        { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/8D", 0.75 }, { "1/4T", 2.0 / 3.0 },
        { "1/4", 1.0 }, { "1/4D", 1.5 }, { "1/2", 2.0 }
    };
    return d;
}

namespace
{
    using namespace juce;
    using TextFn = std::function<String (float, int)>;

    String hzText (float v, int)   { return v < 1000.0f ? String (v, v < 100.0f ? 1 : 0) + " Hz" : String (v / 1000.0f, 2) + " kHz"; }
    String secText (float v, int)  { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; }
    String pctText (float v, int)  { return String (roundToInt (v * 100.0f)) + "%"; }
    String biText (float v, int)   { auto p = roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; }
    String centText (float v, int) { return String (roundToInt (v)) + " ct"; }
    String dbText (float v, int)   { return String (v, 1) + " dB"; }
    String lfoHzText (float v, int){ return String (v, v < 1.0f ? 2 : 1) + " Hz"; }

    NormalisableRange<float> timeRange()
    {
        NormalisableRange<float> r (0.001f, 10.0f);
        r.setSkewForCentre (0.6f);
        return r;
    }

    StringArray divisionNames (const std::vector<Division>& d)
    {
        StringArray s;
        for (auto& x : d) s.add (x.name);
        return s;
    }

    int divisionIndex (const std::vector<Division>& d, const char* name)
    {
        for (size_t i = 0; i < d.size(); ++i)
            if (String (d[i].name) == name) return (int) i;
        return 0;
    }

    struct Builder
    {
        AudioProcessorValueTreeState::ParameterLayout layout;

        void flt (const String& pid, const String& name, NormalisableRange<float> r, float def, TextFn fn)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                                                                AudioParameterFloatAttributes().withStringFromValueFunction (fn)));
        }
        void integer (const String& pid, const String& name, int lo, int hi, int def)
        {
            layout.add (std::make_unique<AudioParameterInt> (ParameterID { pid, 1 }, name, lo, hi, def));
        }
        void choice (const String& pid, const String& name, const StringArray& items, int def)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, items, def));
        }
        void boolean (const String& pid, const String& name, bool def)
        {
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def));
        }
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Builder b;
    const NormalisableRange<float> unit (0.0f, 1.0f);
    const NormalisableRange<float> bipolar (-1.0f, 1.0f);

    for (int o = 0; o < kNumOscs; ++o)
    {
        auto n = "Osc" + String (o + 1) + " ";
        b.boolean (id::osc (o, "on"), n + "On", o == 0);
        b.choice  (id::osc (o, "table"), n + "Wavetable", tableNames(), 0);
        b.flt     (id::osc (o, "pos"), n + "Position", unit, 2.0f / 3.0f, pctText);
        b.integer (id::osc (o, "coarse"), n + "Coarse", -24, 24, 0);
        b.flt     (id::osc (o, "fine"), n + "Fine", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, centText);
        b.flt     (id::osc (o, "level"), n + "Level", unit, 0.8f, pctText);
        b.integer (id::osc (o, "unison"), n + "Unison", 1, kMaxUnison, 1);
        b.flt     (id::osc (o, "detune"), n + "Detune", unit, 0.25f, pctText);
        b.flt     (id::osc (o, "spread"), n + "Spread", unit, 0.6f, pctText);
        b.flt     (id::osc (o, "warp"), n + "Warp", bipolar, 0.0f, biText);
        b.flt     (id::osc (o, "pan"), n + "Pan", bipolar, 0.0f, biText);
    }

    b.flt    ("sub_level", "Sub Level", unit, 0.0f, pctText);
    b.choice ("sub_shape", "Sub Shape", { "Sine", "Square" }, 0);
    b.flt    ("noise_level", "Noise Level", unit, 0.0f, pctText);

    NormalisableRange<float> cutoff (20.0f, 20000.0f);
    cutoff.setSkewForCentre (1000.0f);
    b.choice ("flt_type", "Filter Type", filterTypeNames(), filt::LP24);
    b.flt    ("flt_cutoff", "Cutoff", cutoff, 12000.0f, hzText);
    b.flt    ("flt_res", "Resonance", unit, 0.1f, pctText);
    b.flt    ("flt_drive", "Filter Drive", unit, 0.0f, pctText);
    b.flt    ("flt_env", "Filter Env Amount", bipolar, 0.0f, biText);
    b.flt    ("flt_key", "Filter Keytrack", unit, 0.0f, pctText);

    b.flt ("amp_a", "Amp Attack", timeRange(), 0.005f, secText);
    b.flt ("amp_d", "Amp Decay", timeRange(), 0.4f, secText);
    b.flt ("amp_s", "Amp Sustain", unit, 0.8f, pctText);
    b.flt ("amp_r", "Amp Release", timeRange(), 0.3f, secText);
    b.flt ("mod_a", "Mod Attack", timeRange(), 0.001f, secText);
    b.flt ("mod_d", "Mod Decay", timeRange(), 0.5f, secText);
    b.flt ("mod_s", "Mod Sustain", unit, 0.0f, pctText);
    b.flt ("mod_r", "Mod Release", timeRange(), 0.3f, secText);

    NormalisableRange<float> lfoRate (0.01f, 30.0f);
    lfoRate.setSkewForCentre (2.0f);
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto n = "LFO" + String (l + 1) + " ";
        b.choice  (id::lfo (l, "shape"), n + "Shape", lfoShapeNames(), lfoShape::Sine);
        b.flt     (id::lfo (l, "rate"), n + "Rate", lfoRate, l == 0 ? 1.0f : 0.3f, lfoHzText);
        b.boolean (id::lfo (l, "sync"), n + "Sync", false);
        b.choice  (id::lfo (l, "div"), n + "Division", divisionNames (lfoDivisions()), divisionIndex (lfoDivisions(), "1/4"));
    }

    for (int s = 0; s < kNumSlots; ++s)
    {
        auto n = "Mod " + String (s + 1) + " ";
        b.choice (id::slot (s, "src"), n + "Source", modSourceNames(), s == 0 ? src::ModWheel : src::None);
        b.choice (id::slot (s, "dst"), n + "Destination", modDestNames(), s == 0 ? dst::Cutoff : dst::None);
        b.flt    (id::slot (s, "amt"), n + "Amount", bipolar, s == 0 ? 0.3f : 0.0f, biText);
    }

    for (int m = 0; m < kNumMacros; ++m)
        b.flt (id::macro (m), "Macro " + String (m + 1), unit, 0.0f, pctText);

    b.boolean ("fx_dist_on", "Dist On", false);
    b.flt     ("fx_dist_drive", "Dist Drive", unit, 0.3f, pctText);
    b.flt     ("fx_dist_mix", "Dist Mix", unit, 0.5f, pctText);

    NormalisableRange<float> choRate (0.05f, 5.0f);
    choRate.setSkewForCentre (1.0f);
    b.boolean ("fx_cho_on", "Chorus On", false);
    b.flt     ("fx_cho_rate", "Chorus Rate", choRate, 0.8f, lfoHzText);
    b.flt     ("fx_cho_depth", "Chorus Depth", unit, 0.4f, pctText);
    b.flt     ("fx_cho_mix", "Chorus Mix", unit, 0.4f, pctText);

    b.boolean ("fx_dly_on", "Delay On", false);
    b.choice  ("fx_dly_div", "Delay Time", divisionNames (delayDivisions()), divisionIndex (delayDivisions(), "1/8D"));
    b.flt     ("fx_dly_fb", "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.35f, pctText);
    b.flt     ("fx_dly_mix", "Delay Mix", unit, 0.25f, pctText);

    b.boolean ("fx_rev_on", "Reverb On", true);
    b.flt     ("fx_rev_size", "Reverb Size", unit, 0.6f, pctText);
    b.flt     ("fx_rev_damp", "Reverb Damping", unit, 0.5f, pctText);
    b.flt     ("fx_rev_mix", "Reverb Mix", unit, 0.2f, pctText);

    NormalisableRange<float> glide (0.0f, 2.0f);
    glide.setSkewForCentre (0.2f);
    b.flt     ("master", "Master Volume", NormalisableRange<float> (-48.0f, 6.0f), -6.0f, dbText);
    b.flt     ("glide", "Glide", glide, 0.0f, secText);
    b.boolean ("mono", "Mono / Legato", false);
    b.integer ("bend_range", "Pitch Bend Range", 1, 24, 2);
    b.flt     ("vel_sens", "Velocity Sensitivity", unit, 0.5f, pctText);

    return std::move (b.layout);
}
} // namespace rum
