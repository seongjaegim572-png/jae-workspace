// Offline test: renders every factory preset into one WAV, checks the audio is sane,
// measures CPU speed, checks state save/load, and saves a screenshot of the editor.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace
{
struct Event { double time; int note; bool on; };

std::vector<Event> phrase (bool bass)
{
    std::vector<Event> e;
    const int base = bass ? 36 : 60;
    // chord
    for (int n : { 0, 3, 7, 10 }) { e.push_back ({ 0.0, base + n, true }); e.push_back ({ 1.2, base + n, false }); }
    // overlapping (legato) line
    const int line[] = { 0, 7, 12, 10, 7 };
    for (int i = 0; i < 5; ++i)
    {
        e.push_back ({ 1.4 + i * 0.3, base + line[i], true });
        e.push_back ({ 1.4 + i * 0.3 + 0.36, base + line[i], false });
    }
    return e;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    constexpr double sr = 48000.0;
    constexpr int block = 512;
    RuminatorProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    const int numPresets = proc.getNumPrograms();
    const double secondsPer = 3.6;
    const int samplesPer = (int) (secondsPer * sr);
    juce::AudioBuffer<float> all (2, samplesPer * numPresets);
    all.clear();

    bool ok = true;
    double renderSeconds = 0.0;

    for (int p = 0; p < numPresets; ++p)
    {
        proc.setCurrentProgram (p);
        const auto name = proc.getProgramName (p);
        const bool bass = name.containsIgnoreCase ("bass");
        auto events = phrase (bass);

        juce::AudioBuffer<float> buf (2, block);
        float peak = 0.0f; double sumSq = 0.0; bool finite = true;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();

        for (int pos = 0; pos < samplesPer; pos += block)
        {
            const int n = juce::jmin (block, samplesPer - pos);
            buf.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (auto& e : events)
            {
                const int s = (int) (e.time * sr);
                if (s >= pos && s < pos + n)
                    midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) 100)
                                        : juce::MidiMessage::noteOff (1, e.note), s - pos);
            }
            proc.processBlock (buf, midi);

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
        std::printf ("%-16s peak %.3f  rms %.4f  %s\n", name.toRawUTF8(), peak, rms, good ? "OK" : "FAIL");

        // let tails die between presets
        proc.reset();
        for (int k = 0; k < 40; ++k) { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::allNotesOff (1), 0); proc.processBlock (buf, m); }
    }

    const double audioSeconds = secondsPer * numPresets;
    std::printf ("Rendered %.1f s of audio in %.2f s (%.1fx realtime)\n", audioSeconds, renderSeconds, audioSeconds / renderSeconds);

    // Normalise demo to -1 dBFS and write WAV
    const float demoPeak = all.getMagnitude (0, all.getNumSamples());
    if (demoPeak > 0.0f) all.applyGain (0.89f / demoPeak);
    {
        auto file = outDir.getChildFile ("ruminator_presets_demo.wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (file), sr, 2, 16, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        std::printf ("Wrote %s\n", file.getFullPathName().toRawUTF8());
    }

    // State round trip
    {
        proc.setCurrentProgram (3);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        RuminatorProcessor other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool same = other.getCurrentProgram() == 3
                          && other.apvts.getRawParameterValue ("mono")->load() > 0.5f
                          && std::abs (other.apvts.getRawParameterValue ("glide")->load() - 0.08f) < 1e-4f;
        std::printf ("State save/load: %s\n", same ? "OK" : "FAIL");
        ok = ok && same;
    }

    // Editor screenshot
    {
        proc.setCurrentProgram (1);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (ed->getWidth(), ed->getHeight());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto file = outDir.getChildFile ("ruminator_ui.png");
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
        std::printf ("Wrote %s\n", file.getFullPathName().toRawUTF8());
    }

    std::printf (ok ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return ok ? 0 : 1;
}
