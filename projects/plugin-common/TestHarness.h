#pragma once

// Offline test used by CI: renders every preset, checks the audio is sane,
// checks state save/load, writes a demo WAV and an editor screenshot.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

namespace jae::test
{
struct Note { double time, length; int note; int velocity; };
using PhraseFn = std::function<std::vector<Note> (const juce::String& presetName)>;
using Factory = std::function<std::unique_ptr<juce::AudioProcessor>()>;

inline std::vector<Note> defaultPhrase (int base)
{
    std::vector<Note> e;
    for (int n : { 0, 4, 7, 11 }) e.push_back ({ 0.0, 1.3, base + n, 90 });
    const int line[] = { 0, 7, 12, 14, 11, 7 };
    for (int i = 0; i < 6; ++i) e.push_back ({ 1.5 + i * 0.25, 0.3, base + 12 + line[i], 70 + i * 8 });
    return e;
}

inline int run (const Factory& create, const PhraseFn& phrase, double secondsPer, const juce::String& prefix,
                int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    constexpr double sr = 48000.0;
    constexpr int block = 512;
    auto proc = create();
    proc->setPlayConfigDetails (0, 2, sr, block);
    proc->prepareToPlay (sr, block);

    const int numPresets = proc->getNumPrograms();
    const int samplesPer = (int) (secondsPer * sr);
    juce::AudioBuffer<float> all (2, samplesPer * numPresets);
    all.clear();
    bool ok = true;
    double renderSeconds = 0.0;

    for (int p = 0; p < numPresets; ++p)
    {
        proc->setCurrentProgram (p);
        const auto name = proc->getProgramName (p);
        const auto notes = phrase (name);

        juce::AudioBuffer<float> buf (2, block);
        float peak = 0.0f; double sumSq = 0.0; bool finite = true;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();

        for (int pos = 0; pos < samplesPer; pos += block)
        {
            const int n = juce::jmin (block, samplesPer - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& e : notes)
            {
                const int on = (int) (e.time * sr), off = (int) ((e.time + e.length) * sr);
                if (on >= pos && on < pos + n)
                    midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.velocity), on - pos);
                if (off >= pos && off < pos + n)
                    midi.addEvent (juce::MidiMessage::noteOff (1, e.note), off - pos);
            }
            proc->processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
            {
                const float* d = buf.getReadPointer (c);
                for (int i = 0; i < n; ++i)
                {
                    if (! std::isfinite (d[i])) finite = false;
                    peak = juce::jmax (peak, std::abs (d[i]));
                    sumSq += (double) d[i] * d[i];
                }
                all.copyFrom (c, p * samplesPer + pos, buf, c, 0, n);
            }
        }
        renderSeconds += (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;

        const double rms = std::sqrt (sumSq / (2.0 * samplesPer));
        const bool good = finite && peak > 0.01f && peak < 4.0f;
        ok = ok && good;
        std::printf ("%-20s peak %.3f  rms %.4f  %s\n", name.toRawUTF8(), peak, rms, good ? "OK" : "FAIL");

        // flush tails between presets
        for (int k = 0; k < 60; ++k)
        {
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            buf.setSize (2, block, false, false, true);
            proc->processBlock (buf, m);
        }
    }

    const double audioSeconds = secondsPer * numPresets;
    std::printf ("Rendered %.1f s of audio in %.2f s (%.1fx realtime)\n", audioSeconds, renderSeconds, audioSeconds / renderSeconds);

    const float demoPeak = all.getMagnitude (0, all.getNumSamples());
    if (demoPeak > 0.0f) all.applyGain (0.89f / demoPeak);
    {
        auto file = outDir.getChildFile (prefix + "_presets_demo.wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (file), sr, 2, 16, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }

    // state round trip: every parameter must survive save/load
    {
        proc->setCurrentProgram (juce::jmin (2, numPresets - 1));
        juce::MemoryBlock mb;
        proc->getStateInformation (mb);
        auto other = create();
        other->setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = other->getCurrentProgram() == proc->getCurrentProgram();
        auto& a = proc->getParameters();
        auto& b = other->getParameters();
        same = same && a.size() == b.size();
        for (int i = 0; same && i < a.size(); ++i)
            same = std::abs (a[i]->getValue() - b[i]->getValue()) < 1.0e-5f;
        std::printf ("State save/load: %s\n", same ? "OK" : "FAIL");
        ok = ok && same;
    }

    {
        proc->setCurrentProgram (1);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto file = outDir.getChildFile (prefix + "_ui.png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
    }

    std::printf (ok ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return ok ? 0 : 1;
}
} // namespace jae::test
