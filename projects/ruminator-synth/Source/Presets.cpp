#include "Presets.h"
#include "Params.h"

namespace rum
{
namespace
{
    // Wavetable indices
    constexpr float kBasic = 0, kPwm = 1, kSync = 2, kHarmonic = 3, kFormant = 4, kDigital = 5, kOrgan = 6, kFold = 7;
    // Positions inside the "Basic" table
    constexpr float kSine = 0.0f, kTri = 1.0f / 3.0f, kSaw = 2.0f / 3.0f, kSquare = 1.0f;

    float S (int v) { return (float) v; }
}

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init", {} },

        { "Hyper Supersaw", {
            { "osc1_table", kBasic }, { "osc1_pos", kSaw }, { "osc1_unison", 7 }, { "osc1_detune", 0.35f }, { "osc1_spread", 1.0f },
            { "osc2_on", 1 }, { "osc2_table", kBasic }, { "osc2_pos", kSaw }, { "osc2_coarse", 12 }, { "osc2_unison", 5 },
            { "osc2_detune", 0.25f }, { "osc2_level", 0.45f },
            { "flt_type", S (filt::LP24) }, { "flt_cutoff", 9000.0f }, { "flt_res", 0.15f },
            { "amp_a", 0.005f }, { "amp_d", 0.3f }, { "amp_s", 0.85f }, { "amp_r", 0.35f },
            { "fx_cho_on", 1 }, { "fx_dly_on", 1 }, { "fx_dly_div", 6 }, { "fx_dly_mix", 0.15f },
            { "fx_rev_mix", 0.22f } } },

        { "Digicore Pluck", {
            { "osc1_table", kDigital }, { "osc1_pos", 0.3f }, { "osc1_unison", 3 }, { "osc1_detune", 0.15f },
            { "osc2_on", 1 }, { "osc2_table", kPwm }, { "osc2_pos", 0.4f }, { "osc2_coarse", 12 }, { "osc2_level", 0.4f },
            { "flt_type", S (filt::LP24) }, { "flt_cutoff", 500.0f }, { "flt_res", 0.35f }, { "flt_env", 0.65f },
            { "mod_a", 0.001f }, { "mod_d", 0.25f }, { "mod_s", 0.0f }, { "mod_r", 0.2f },
            { "amp_a", 0.001f }, { "amp_d", 0.45f }, { "amp_s", 0.0f }, { "amp_r", 0.3f },
            { "mm2_src", S (src::Velocity) }, { "mm2_dst", S (dst::Cutoff) }, { "mm2_amt", 0.3f },
            { "mm3_src", S (src::Macro1) }, { "mm3_dst", S (dst::Osc1Pos) }, { "mm3_amt", 0.6f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 5 }, { "fx_dly_fb", 0.35f }, { "fx_dly_mix", 0.25f },
            { "fx_rev_mix", 0.25f } } },

        { "808 Slide Bass", {
            { "mono", 1 }, { "glide", 0.08f },
            { "osc1_table", kBasic }, { "osc1_pos", kSine }, { "osc1_level", 1.0f },
            { "sub_level", 0.4f },
            { "flt_type", S (filt::LP12) }, { "flt_cutoff", 3000.0f }, { "flt_res", 0.0f },
            { "amp_a", 0.001f }, { "amp_d", 1.6f }, { "amp_s", 0.0f }, { "amp_r", 0.25f },
            { "mod_a", 0.001f }, { "mod_d", 0.06f }, { "mod_s", 0.0f },
            { "mm2_src", S (src::ModEnv) }, { "mm2_dst", S (dst::AllPitch) }, { "mm2_amt", 0.5f },
            { "fx_dist_on", 1 }, { "fx_dist_drive", 0.35f }, { "fx_dist_mix", 0.5f },
            { "fx_rev_on", 0 } } },

        { "Formant Lead", {
            { "mono", 1 }, { "glide", 0.06f },
            { "osc1_table", kFormant }, { "osc1_pos", 0.2f }, { "osc1_unison", 3 }, { "osc1_detune", 0.12f },
            { "osc2_on", 1 }, { "osc2_table", kBasic }, { "osc2_pos", kSaw }, { "osc2_level", 0.35f }, { "osc2_coarse", -12 },
            { "flt_type", S (filt::LP12) }, { "flt_cutoff", 7000.0f },
            { "lfo1_sync", 1 }, { "lfo1_div", 3 },
            { "mm2_src", S (src::Lfo1) }, { "mm2_dst", S (dst::Osc1Pos) }, { "mm2_amt", 0.4f },
            { "mm3_src", S (src::Lfo2) }, { "mm3_dst", S (dst::AllPitch) }, { "mm3_amt", 0.006f },
            { "lfo2_rate", 5.5f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 8 }, { "fx_dly_mix", 0.2f }, { "fx_rev_mix", 0.25f } } },

        { "Lo-fi Keys", {
            { "osc1_table", kOrgan }, { "osc1_pos", 0.3f },
            { "osc2_on", 1 }, { "osc2_table", kBasic }, { "osc2_pos", kTri }, { "osc2_level", 0.45f },
            { "flt_type", S (filt::LP12) }, { "flt_cutoff", 2400.0f }, { "flt_key", 0.3f },
            { "amp_a", 0.004f }, { "amp_d", 1.2f }, { "amp_s", 0.35f }, { "amp_r", 0.45f },
            { "lfo2_rate", 0.6f },
            { "mm2_src", S (src::Lfo2) }, { "mm2_dst", S (dst::AllPitch) }, { "mm2_amt", 0.005f },
            { "fx_dist_on", 1 }, { "fx_dist_drive", 0.15f }, { "fx_dist_mix", 0.3f },
            { "fx_cho_on", 1 }, { "fx_cho_mix", 0.35f }, { "fx_rev_mix", 0.3f } } },

        { "Dream Pad", {
            { "osc1_table", kHarmonic }, { "osc1_pos", 0.4f }, { "osc1_unison", 5 }, { "osc1_detune", 0.2f }, { "osc1_spread", 0.9f },
            { "osc2_on", 1 }, { "osc2_table", kSync }, { "osc2_pos", 0.2f }, { "osc2_unison", 3 }, { "osc2_level", 0.4f }, { "osc2_coarse", 12 },
            { "flt_type", S (filt::LP24) }, { "flt_cutoff", 2500.0f }, { "flt_res", 0.2f },
            { "amp_a", 1.2f }, { "amp_d", 1.0f }, { "amp_s", 0.8f }, { "amp_r", 2.5f },
            { "lfo1_rate", 0.15f }, { "lfo2_rate", 0.1f }, { "lfo2_shape", S (lfoShape::Triangle) },
            { "mm2_src", S (src::Lfo1) }, { "mm2_dst", S (dst::Osc1Pos) }, { "mm2_amt", 0.3f },
            { "mm3_src", S (src::Lfo2) }, { "mm3_dst", S (dst::Cutoff) }, { "mm3_amt", 0.15f },
            { "fx_cho_on", 1 }, { "fx_rev_size", 0.88f }, { "fx_rev_mix", 0.45f } } },

        { "Rage Lead", {
            { "osc1_table", kSync }, { "osc1_pos", 0.5f }, { "osc1_unison", 5 }, { "osc1_detune", 0.3f }, { "osc1_spread", 0.8f },
            { "osc2_on", 1 }, { "osc2_table", kBasic }, { "osc2_pos", kSaw }, { "osc2_coarse", -12 }, { "osc2_unison", 3 }, { "osc2_level", 0.5f },
            { "flt_type", S (filt::LP24) }, { "flt_cutoff", 6000.0f }, { "flt_res", 0.25f },
            { "amp_a", 0.003f }, { "amp_d", 0.4f }, { "amp_s", 0.7f }, { "amp_r", 0.25f },
            { "mm2_src", S (src::Macro1) }, { "mm2_dst", S (dst::Osc1Pos) }, { "mm2_amt", 0.5f },
            { "fx_dist_on", 1 }, { "fx_dist_drive", 0.5f }, { "fx_dist_mix", 0.4f },
            { "fx_dly_on", 1 }, { "fx_dly_div", 2 }, { "fx_dly_mix", 0.12f }, { "fx_rev_mix", 0.2f } } },

        { "Wobble Bass", {
            { "mono", 1 },
            { "osc1_table", kFold }, { "osc1_pos", 0.4f },
            { "osc2_on", 1 }, { "osc2_table", kBasic }, { "osc2_pos", kSquare }, { "osc2_coarse", -12 }, { "osc2_level", 0.5f },
            { "flt_type", S (filt::LP24) }, { "flt_cutoff", 350.0f }, { "flt_res", 0.4f },
            { "lfo1_sync", 1 }, { "lfo1_div", 8 },
            { "mm2_src", S (src::Lfo1) }, { "mm2_dst", S (dst::Cutoff) }, { "mm2_amt", 0.6f },
            { "mm3_src", S (src::Lfo1) }, { "mm3_dst", S (dst::Osc1Pos) }, { "mm3_amt", 0.3f },
            { "fx_dist_on", 1 }, { "fx_dist_drive", 0.4f }, { "fx_dist_mix", 0.6f }, { "fx_rev_on", 0 } } },
    };
    return presets;
}

void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& preset)
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
} // namespace rum
