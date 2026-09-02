// LUCID PULSE - headless plug-in tests: instrument renders all presets, MIDI build emits notes, state round trip
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include "../Pulse/PulseProcessor.h"
#include <cstdio>
#include <map>
using namespace pulse;
static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (!(cond)) { ++failures; std::printf ("  FAIL: %s\n", juce::String (msg).toRawUTF8()); } else std::printf ("  ok:   %s\n", juce::String (msg).toRawUTF8()); } while (0)

struct Head : public juce::AudioPlayHead
{
    double ppq = 0.0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (126.0); p.setPpqPosition (ppq); p.setIsPlaying (playing); p.setTimeSignature (TimeSignature { 4, 4 }); return p;
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("LUCID PULSE plug-in tests (%s build)\n", PulseProcessor::isMidiOnlyBuild() ? "MIDI" : "instrument");
    PulseProcessor proc; Head head; proc.setPlayHead (&head);
    proc.setPlayConfigDetails (0, PulseProcessor::isMidiOnlyBuild() ? 0 : 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    std::printf ("  %d parameters\n", proc.getParameters().size());
    const double spb = 48000.0 * 60.0 / 126.0;

    for (int pi = 0; pi < proc.getNumPrograms(); ++pi)
    {
        proc.loadPreset (pi);
        head.ppq = 0.0;
        int noteOns = 0; double rms = 0; int n = 0; float peak = 0; bool finite = true; std::map<int, int> perNote;
        for (int b = 0; b < 2400; ++b)
        {
            juce::AudioBuffer<float> buf (PulseProcessor::isMidiOnlyBuild() ? 0 : 2, 512); juce::MidiBuffer midi;
            buf.clear(); proc.processBlock (buf, midi);
            for (const auto m : midi) if (m.getMessage().isNoteOn()) { ++noteOns; ++perNote[m.getMessage().getNoteNumber()]; }
            if (buf.getNumChannels() > 0) { const float* d = buf.getReadPointer (0); for (int i = 0; i < 512; ++i) { rms += d[i] * d[i]; ++n; peak = std::max (peak, std::fabs (d[i])); if (! std::isfinite (d[i])) finite = false; } }
            head.ppq += 512.0 / spb;
        }
        rms = n > 0 ? std::sqrt (rms / n) : 0.0;
        if (PulseProcessor::isMidiOnlyBuild()) { std::printf ("    per note:"); for (auto& kv : perNote) std::printf (" %d:%d", kv.first, kv.second); std::printf ("\n"); }
        juce::String msg = juce::String (proc.getProgramName (pi)) + (PulseProcessor::isMidiOnlyBuild() ? juce::String::formatted ("  (%d note-ons in ~13 bars)", noteOns) : juce::String::formatted ("  (rms %.3f, peak %.2f)", rms, peak));
        if (PulseProcessor::isMidiOnlyBuild()) CHECK (noteOns > 100, msg); else CHECK (finite && rms > 0.01 && peak <= 1.0f, msg);
    }
    // host stopped + host sync -> silence / no notes
    {
        head.playing = false; int ons = 0; double e = 0;
        for (int b = 0; b < 100; ++b) { juce::AudioBuffer<float> buf (PulseProcessor::isMidiOnlyBuild() ? 0 : 2, 512); juce::MidiBuffer midi; buf.clear(); proc.processBlock (buf, midi); for (const auto m : midi) ons += m.getMessage().isNoteOn(); if (buf.getNumChannels() > 0) for (int i = 0; i < 512; ++i) e += std::fabs (buf.getSample (0, i)); }
        CHECK (ons == 0, "no new notes while the host is stopped (host sync)");
        head.playing = true;
    }
    // state round trip incl. pattern data
    {
        proc.step (0, 2, 5).store (2);
        proc.step (1, 7, 31).store (4);
        auto* sw = proc.apvts.getParameter ("swing"); sw->setValueNotifyingHost (sw->convertTo0to1 (0.42f));
        juce::MemoryBlock state; proc.getStateInformation (state);
        proc.loadInit();
        CHECK (proc.getStep (0, 2, 5) == 0, "init clears the grid");
        proc.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (proc.getStep (0, 2, 5) == 2 && proc.getStep (1, 7, 31) == 4, "step data restored from state");
        CHECK (std::fabs (sw->convertFrom0to1 (sw->getValue()) - 0.42f) < 0.001f, "swing restored from state");
    }
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
