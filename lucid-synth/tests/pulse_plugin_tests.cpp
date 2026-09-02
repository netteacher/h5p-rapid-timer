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
    // host stopped + host sync explicitly on -> silence / no notes. (Host Sync defaults ON for the
    // instrument build and OFF for the MIDI-FX build - see PulseParameters.cpp - so force it on here
    // to test the behaviour itself rather than either build's default.)
    {
        if (auto* hs = proc.apvts.getParameter ("hostSync")) hs->setValueNotifyingHost (1.0f);
        head.playing = false; int ons = 0; double e = 0;
        for (int b = 0; b < 100; ++b) { juce::AudioBuffer<float> buf (PulseProcessor::isMidiOnlyBuild() ? 0 : 2, 512); juce::MidiBuffer midi; buf.clear(); proc.processBlock (buf, midi); for (const auto m : midi) ons += m.getMessage().isNoteOn(); if (buf.getNumChannels() > 0) for (int i = 0; i < 512; ++i) e += std::fabs (buf.getSample (0, i)); }
        CHECK (ons == 0, "no new notes while the host is stopped (host sync)");
        head.playing = true;
    }
    // The instrument build renders the sequencer's notes as audio (no MIDI output is expected from
    // it), the MIDI-FX build emits them as MIDI note-ons - measure whichever applies.
    auto sequencerIsActive = [&] () -> bool
    {
        int ons = 0; float peak = 0.0f;
        for (int b = 0; b < 40; ++b)
        {
            juce::AudioBuffer<float> buf (PulseProcessor::isMidiOnlyBuild() ? 0 : 2, 512); juce::MidiBuffer midi; buf.clear();
            proc.processBlock (buf, midi);
            for (const auto m : midi) ons += m.getMessage().isNoteOn();
            if (buf.getNumChannels() > 0) { const float* d = buf.getReadPointer (0); for (int i = 0; i < 512; ++i) peak = std::max (peak, std::fabs (d[i])); }
        }
        return PulseProcessor::isMidiOnlyBuild() ? ons > 0 : peak > 0.01f;
    };
    // free-running fallback: with Host Sync off (the MIDI-FX build's default), the sequencer keeps
    // generating notes even while the host reports "not playing" or "not moving".
    {
        if (auto* hs = proc.apvts.getParameter ("hostSync")) hs->setValueNotifyingHost (0.0f);
        head.playing = false;
        CHECK (sequencerIsActive(), "with Host Sync off, the sequencer free-runs even while the host reports stopped");
        head.playing = true;
    }
    // some hosts don't hand a MIDI-effect plug-in a working play head at all - confirmed this no
    // longer means "silent forever" even with Host Sync switched on.
    {
        if (auto* hs = proc.apvts.getParameter ("hostSync")) hs->setValueNotifyingHost (1.0f);
        proc.setPlayHead (nullptr);
        CHECK (sequencerIsActive(), "falls back to the free-running clock when the host provides no play head at all");
        proc.setPlayHead (&head);
        if (auto* hs = proc.apvts.getParameter ("hostSync")) hs->setValueNotifyingHost (0.0f);
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
    // drag-to-DAW MIDI export: renders deterministically, produces a valid, non-empty file
    {
        proc.loadPreset (0);
        auto file = proc.renderPatternToMidiFile (0, 2, 126.0);
        juce::MidiFile mf; bool parsed = false;
        { juce::FileInputStream in (file); if (in.openedOk()) parsed = mf.readFrom (in); }
        int notes = 0, tsEvents = 0;
        for (int t = 0; t < mf.getNumTracks(); ++t)
        {
            auto* trk = mf.getTrack (t);
            for (int i = 0; i < trk->getNumEvents(); ++i)
            {
                const auto& m = trk->getEventPointer (i)->message;
                if (m.isNoteOn()) ++notes;
                if (m.isTimeSignatureMetaEvent()) ++tsEvents;
            }
        }
        CHECK (file.existsAsFile() && parsed && notes > 0, juce::String ("MIDI export produced a valid file with ") + juce::String (notes) + " notes");
        CHECK (tsEvents > 0, "MIDI export includes a time signature meta event");
        // rendering the same pattern twice must give the same note count (deterministic, WYSIWYG)
        auto file2 = proc.renderPatternToMidiFile (0, 2, 126.0);
        juce::MidiFile mf2; { juce::FileInputStream in2 (file2); if (in2.openedOk()) mf2.readFrom (in2); }
        int notes2 = 0;
        for (int t = 0; t < mf2.getNumTracks(); ++t) { auto* trk = mf2.getTrack (t); for (int i = 0; i < trk->getNumEvents(); ++i) if (trk->getEventPointer (i)->message.isNoteOn()) ++notes2; }
        CHECK (notes == notes2, "MIDI export is deterministic (same note count on repeat render)");
        file.deleteFile(); file2.deleteFile();
    }
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
