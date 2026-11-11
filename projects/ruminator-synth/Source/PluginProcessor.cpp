#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "Wavetables.h"

using namespace rum;

RuminatorProcessor::RuminatorProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RUMINATOR", createParameterLayout())
{
    WavetableBank::get(); // build tables now, never on the audio thread

    for (int o = 0; o < kNumOscs; ++o)
    {
        auto& p = oscP[(size_t) o];
        p.on = raw (id::osc (o, "on"));         p.table = raw (id::osc (o, "table"));
        p.pos = raw (id::osc (o, "pos"));       p.coarse = raw (id::osc (o, "coarse"));
        p.fine = raw (id::osc (o, "fine"));     p.level = raw (id::osc (o, "level"));
        p.unison = raw (id::osc (o, "unison")); p.detune = raw (id::osc (o, "detune"));
        p.spread = raw (id::osc (o, "spread")); p.warp = raw (id::osc (o, "warp"));
        p.pan = raw (id::osc (o, "pan"));
    }

    subLevel = raw ("sub_level"); subShape = raw ("sub_shape"); noiseLevel = raw ("noise_level");
    fltType = raw ("flt_type"); cutoff = raw ("flt_cutoff"); res = raw ("flt_res");
    drive = raw ("flt_drive"); fltEnv = raw ("flt_env"); fltKey = raw ("flt_key");
    ampA = raw ("amp_a"); ampD = raw ("amp_d"); ampS = raw ("amp_s"); ampR = raw ("amp_r");
    modA = raw ("mod_a"); modD = raw ("mod_d"); modS = raw ("mod_s"); modR = raw ("mod_r");

    for (int l = 0; l < kNumLfos; ++l)
    {
        lfoShapeP[(size_t) l] = raw (id::lfo (l, "shape"));
        lfoRateP[(size_t) l] = raw (id::lfo (l, "rate"));
        lfoSyncP[(size_t) l] = raw (id::lfo (l, "sync"));
        lfoDivP[(size_t) l] = raw (id::lfo (l, "div"));
    }
    for (int s = 0; s < kNumSlots; ++s)
    {
        slotSrcP[(size_t) s] = raw (id::slot (s, "src"));
        slotDstP[(size_t) s] = raw (id::slot (s, "dst"));
        slotAmtP[(size_t) s] = raw (id::slot (s, "amt"));
    }
    for (int m = 0; m < kNumMacros; ++m) macroP[(size_t) m] = raw (id::macro (m));

    distOn = raw ("fx_dist_on"); distDrive = raw ("fx_dist_drive"); distMix = raw ("fx_dist_mix");
    choOn = raw ("fx_cho_on"); choRate = raw ("fx_cho_rate"); choDepth = raw ("fx_cho_depth"); choMix = raw ("fx_cho_mix");
    dlyOn = raw ("fx_dly_on"); dlyDiv = raw ("fx_dly_div"); dlyFb = raw ("fx_dly_fb"); dlyMix = raw ("fx_dly_mix");
    revOn = raw ("fx_rev_on"); revSize = raw ("fx_rev_size"); revDamp = raw ("fx_rev_damp"); revMix = raw ("fx_rev_mix");
    master = raw ("master"); glide = raw ("glide"); mono = raw ("mono"); bendRange = raw ("bend_range"); velSens = raw ("vel_sens");

    heldNotes.reserve (128);
}

bool RuminatorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RuminatorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (size_t i = 0; i < voices.size(); ++i)
        voices[i].prepare (sampleRate, 0x9E3779B9u * (uint32_t) (i + 1));

    effects.prepare (sampleRate, samplesPerBlock);
    effects.reset();
    masterGain.reset (sampleRate, 0.03);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (master->load()));
    heldNotes.clear();
    sustainDown = false;
}

void RuminatorProcessor::readSettings (double bpm)
{
    auto b = [] (std::atomic<float>* p) { return p->load() > 0.5f; };
    auto i = [] (std::atomic<float>* p) { return juce::roundToInt (p->load()); };

    for (int o = 0; o < kNumOscs; ++o)
    {
        auto& p = oscP[(size_t) o];
        auto& s = settings.osc[(size_t) o];
        s.on = b (p.on); s.table = i (p.table); s.pos = p.pos->load();
        s.coarse = (float) i (p.coarse); s.fine = p.fine->load(); s.level = p.level->load();
        s.unison = i (p.unison); s.detune = p.detune->load(); s.spread = p.spread->load();
        s.warp = p.warp->load(); s.pan = p.pan->load();
    }

    settings.subLevel = subLevel->load(); settings.subShape = i (subShape); settings.noiseLevel = noiseLevel->load();
    settings.fltType = i (fltType); settings.cutoff = cutoff->load(); settings.res = res->load();
    settings.drive = drive->load(); settings.fltEnv = fltEnv->load(); settings.fltKey = fltKey->load();
    settings.ampA = ampA->load(); settings.ampD = ampD->load(); settings.ampS = ampS->load(); settings.ampR = ampR->load();
    settings.modA = modA->load(); settings.modD = modD->load(); settings.modS = modS->load(); settings.modR = modR->load();

    const auto& divs = lfoDivisions();
    for (int l = 0; l < kNumLfos; ++l)
    {
        settings.lfoShape[(size_t) l] = i (lfoShapeP[(size_t) l]);
        if (b (lfoSyncP[(size_t) l]))
        {
            const auto d = divs[(size_t) juce::jlimit (0, (int) divs.size() - 1, i (lfoDivP[(size_t) l]))];
            settings.lfoRate[(size_t) l] = (float) (bpm / 60.0 / d.beats);
        }
        else
        {
            settings.lfoRate[(size_t) l] = lfoRateP[(size_t) l]->load();
        }
    }

    for (int s = 0; s < kNumSlots; ++s)
    {
        settings.slotSrc[(size_t) s] = i (slotSrcP[(size_t) s]);
        settings.slotDst[(size_t) s] = i (slotDstP[(size_t) s]);
        settings.slotAmt[(size_t) s] = slotAmtP[(size_t) s]->load();
    }
    for (int m = 0; m < kNumMacros; ++m) settings.macros[(size_t) m] = macroP[(size_t) m]->load();

    settings.glide = glide->load();
    settings.mono = b (mono);
    settings.velSens = velSens->load();

    fx.distOn = b (distOn); fx.distDrive = distDrive->load(); fx.distMix = distMix->load();
    fx.choOn = b (choOn); fx.choRate = choRate->load(); fx.choDepth = choDepth->load(); fx.choMix = choMix->load();
    fx.dlyOn = b (dlyOn); fx.dlyFb = dlyFb->load(); fx.dlyMix = dlyMix->load();
    const auto& dd = delayDivisions();
    fx.dlyBeats = dd[(size_t) juce::jlimit (0, (int) dd.size() - 1, i (dlyDiv))].beats;
    fx.revOn = b (revOn); fx.revSize = revSize->load(); fx.revDamp = revDamp->load(); fx.revMix = revMix->load();
}

void RuminatorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                if (*hostBpm > 20.0) bpm = *hostBpm;

    readSettings (bpm);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, numSamples, meta.samplePosition);
        renderVoices (L, R, pos, t);
        handleMidi (meta.getMessage());
        pos = t;
    }
    renderVoices (L, R, pos, numSamples);
    midi.clear();

    effects.process (buffer, fx, bpm);

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (master->load(), -48.0f));
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGain.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }
}

void RuminatorProcessor::renderVoices (float* L, float* R, int start, int end)
{
    if (end <= start) return;
    ctrl.bendSemis = bendNorm * (float) juce::roundToInt (bendRange->load());
    for (auto& v : voices)
        v.render (L + start, R + start, end - start, settings, ctrl);
}

void RuminatorProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        bendNorm = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isChannelPressure())
        ctrl.aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
        ctrl.aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allNotesOff();
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int val = m.getControllerValue();
        if (cc == 1)
            ctrl.modWheel = (float) val / 127.0f;
        else if (cc == 64)
        {
            const bool down = val >= 64;
            if (sustainDown && ! down)
                for (auto& v : voices) v.pedalUp();
            sustainDown = down;
        }
    }
}

void RuminatorProcessor::noteOn (int note, float velocity)
{
    if (settings.mono)
    {
        auto& v = voices[0];
        const bool legato = ! heldNotes.empty() && v.isActive() && v.isHeld();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        heldNotes.push_back (note);

        if (legato)
            v.glideTo (note);
        else
            v.start (note, velocity, (float) note, ++noteCounter, settings);

        // in mono mode only voice 0 sings
        for (size_t i = 1; i < voices.size(); ++i)
            if (voices[i].isActive()) voices[i].keyUp (false);

        lastNote = note;
        return;
    }

    Voice* chosen = nullptr;
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

    const float from = (settings.glide > 0.0005f && lastNote >= 0) ? (float) lastNote : (float) note;
    chosen->start (note, velocity, from, ++noteCounter, settings);
    lastNote = note;
}

void RuminatorProcessor::noteOff (int note)
{
    if (settings.mono)
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        auto& v = voices[0];
        if (v.isActive() && v.isHeld() && v.getNote() == note)
        {
            if (! heldNotes.empty())
                v.glideTo (heldNotes.back());
            else
                v.keyUp (sustainDown);
        }
    }

    // also covers notes left over from poly mode
    for (auto& v : voices)
        if (v.isActive() && v.isHeld() && v.getNote() == note && ! (settings.mono && &v == &voices[0]))
            v.keyUp (sustainDown);
}

void RuminatorProcessor::allNotesOff()
{
    heldNotes.clear();
    for (auto& v : voices) v.keyUp (false);
}

//==============================================================================
int RuminatorProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String RuminatorProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void RuminatorProcessor::setCurrentProgram (int index)
{
    const auto& p = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) p.size()) || index == currentProgram) return;
    currentProgram = index;
    applyPreset (apvts, p[(size_t) index]);
}

void RuminatorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RuminatorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* RuminatorProcessor::createEditor()
{
    return new RuminatorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RuminatorProcessor();
}
