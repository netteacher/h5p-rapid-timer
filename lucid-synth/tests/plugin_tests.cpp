// LUCID Synth - plug-in level tests: parameter layout, every factory preset renders sound
// through the real AudioProcessor, state save/restore round trip, MIDI handling.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>

using namespace lucid;
static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (!(cond)) { ++failures; std::printf ("  FAIL: %s\n", juce::String (msg).toRawUTF8()); } else std::printf ("  ok:   %s\n", juce::String (msg).toRawUTF8()); } while (0)

static float renderNote (LucidAudioProcessor& proc, int note, int blocks, int holdBlocks, float& peak, bool& finite)
{
    const int block = 512;
    juce::AudioBuffer<float> buf (2, block);
    double sum = 0.0; int count = 0; peak = 0.0f; finite = true;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);
        if (b == holdBlocks) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        buf.clear();
        proc.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
        {
            const float* d = buf.getReadPointer (c);
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (d[i])) finite = false;
                peak = std::max (peak, std::fabs (d[i]));
                if (b < holdBlocks) { sum += (double) d[i] * d[i]; ++count; }
            }
        }
    }
    return count > 0 ? (float) std::sqrt (sum / (double) count) : 0.0f;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("LUCID plug-in tests\n");

    LucidAudioProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // ---- parameter layout
    {
        std::printf ("[parameters]\n");
        const int n = proc.getParameters().size();
        std::printf ("  %d parameters\n", n);
        CHECK (n > 150, "parameter layout created");
        bool idsOk = true;
        for (const auto& preset : factoryPresets())
            for (const auto& v : preset.values)
            {
                auto* p = proc.apvts.getParameter (v.id);
                if (p == nullptr) { idsOk = false; std::printf ("  unknown id '%s' in preset '%s'\n", v.id, preset.name); continue; }
                const auto& range = p->getNormalisableRange();
                if (v.value < range.start - 1.0e-4f || v.value > range.end + 1.0e-4f)
                { idsOk = false; std::printf ("  value %g out of range [%g, %g] for '%s' in preset '%s'\n", v.value, range.start, range.end, v.id, preset.name); }
            }
        CHECK (idsOk, "all factory preset ids exist and values are in range");
    }

    // ---- every factory preset makes a finite, non-silent, non-clipping sound
    {
        std::printf ("[presets]\n");
        for (int i = 0; i < proc.presets.getNumPresets(); ++i)
        {
            proc.presets.loadPreset (i);
            float peak; bool finite;
            const float rms = renderNote (proc, 60, 120, 60, peak, finite);
            // silence the tail before the next preset
            for (int k = 0; k < 40; ++k) { juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; if (k == 0) m.addEvent (juce::MidiMessage::allNotesOff (1), 0); b.clear(); proc.processBlock (b, m); }
            juce::String msg = proc.presets.getName (i) + juce::String::formatted ("  (rms %.3f, peak %.2f)", rms, peak);
            CHECK (finite && rms > 0.002f && peak <= 1.0f, msg);
        }
        proc.presets.loadInit();
    }

    // ---- state round trip
    {
        std::printf ("[state]\n");
        auto* cutoff = proc.apvts.getParameter ("f1_cutoff");
        auto* table = proc.apvts.getParameter ("oscA_table");
        cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (1234.0f));
        table->setValueNotifyingHost (table->convertTo0to1 (7.0f));
        proc.presets.setCurrentName ("RoundTrip");
        juce::MemoryBlock state;
        proc.getStateInformation (state);
        proc.presets.loadInit();
        CHECK (std::fabs (cutoff->convertFrom0to1 (cutoff->getValue()) - 8000.0f) < 1.0f, "init restores default cutoff");
        proc.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (std::fabs (cutoff->convertFrom0to1 (cutoff->getValue()) - 1234.0f) < 1.0f, "cutoff restored from state");
        CHECK ((int) table->convertFrom0to1 (table->getValue()) == 7, "wavetable choice restored from state");
        CHECK (proc.presets.getCurrentName() == "RoundTrip", "preset name restored from state");
    }

    // ---- MIDI: pitch bend, mod wheel, sustain pedal reach the engine without crashing; latency reported
    {
        std::printf ("[midi]\n");
        proc.presets.loadInit();
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 3);
        midi.addEvent (juce::MidiMessage::pitchWheel (1, 12000), 100);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 100), 200);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 300);
        midi.addEvent (juce::MidiMessage::noteOff (1, 64), 400);
        buf.clear(); proc.processBlock (buf, midi);
        for (int k = 0; k < 10; ++k) { juce::MidiBuffer m; buf.clear(); proc.processBlock (buf, m); }
        CHECK (proc.activeVoices.load() == 1, "sustain pedal keeps the voice alive after note off");
        juce::MidiBuffer up; up.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
        buf.clear(); proc.processBlock (buf, up);
        for (int k = 0; k < 120; ++k) { juce::MidiBuffer m; buf.clear(); proc.processBlock (buf, m); }
        CHECK (proc.activeVoices.load() == 0, "voice released after pedal up");
        CHECK (proc.getLatencySamples() == 96, "limiter latency (2 ms @ 48k) reported to host");
    }

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
