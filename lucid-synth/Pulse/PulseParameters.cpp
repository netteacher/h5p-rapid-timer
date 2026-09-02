#include "PulseParameters.h"

namespace pulse {
using APF = juce::AudioParameterFloat; using API = juce::AudioParameterInt; using APB = juce::AudioParameterBool; using APC = juce::AudioParameterChoice;
using Range = juce::NormalisableRange<float>;

static juce::String fmtPct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
static juce::String fmtBipolar (float v, int) { return (v >= 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
static juce::String fmtMs (float v, int) { return (v >= 0 ? "+" : "") + juce::String (v, 1) + " ms"; }
static juce::String fmtDb (float v, int) { return juce::String (v, 1) + " dB"; }
static juce::String fmtSemi (float v, int) { return (v >= 0 ? "+" : "") + juce::String (v, 1) + " st"; }
static juce::String fmtNote (float v, int)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = juce::roundToInt (v); return juce::String (names[n % 12]) + juce::String (n / 12 - 2) + " (" + juce::String (n) + ")";
}

juce::AudioProcessorValueTreeState::ParameterLayout createPulseLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };
    auto addF = [&] (const juce::String& id, const juce::String& name, Range r, float def, juce::String (*fmt) (float, int) = nullptr)
    {
        auto attr = juce::AudioParameterFloatAttributes(); if (fmt) attr = attr.withStringFromValueFunction (fmt);
        layout.add (std::make_unique<APF> (pid (id), name, r, def, attr));
    };
    auto addI = [&] (const juce::String& id, const juce::String& name, int lo, int hi, int def) { layout.add (std::make_unique<API> (pid (id), name, lo, hi, def)); };
    auto addB = [&] (const juce::String& id, const juce::String& name, bool def) { layout.add (std::make_unique<APB> (pid (id), name, def)); };
    auto addC = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& c, int def) { layout.add (std::make_unique<APC> (pid (id), name, c, def)); };

    addB ("run", "Run", true);
    addB ("hostSync", "Host Sync", true);
    addF ("swing", "Swing", Range (0.0f, 1.0f, 0.001f), 0.12f, fmtPct);
    addF ("humTime", "Humanize Time", Range (0.0f, 1.0f, 0.001f), 0.1f, fmtPct);
    addF ("humVel", "Humanize Velocity", Range (0.0f, 1.0f, 0.001f), 0.15f, fmtPct);
    addF ("density", "Density", Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
    addB ("fill", "Fill", false);
    addC ("pattern", "Pattern", { "A", "B", "C", "D" }, 0);
    addF ("master", "Master", Range (-60.0f, 12.0f, 0.1f), 0.0f, fmtDb);
    addF ("busDrive", "Bus Drive", Range (0.0f, 1.0f, 0.001f), 0.15f, fmtPct);
    addB ("busComp", "Bus Compressor", true);
    addF ("busThresh", "Bus Threshold", Range (-40.0f, 0.0f, 0.1f), -12.0f, fmtDb);
    addB ("busLimit", "Bus Limiter", true);

    static const int defaultNotes[kNumLanes] = { 36, 38, 39, 42, 46, 47, 37, 49 };
    static const float defaultLevel[kNumLanes] = { 0.85f, 0.6f, 0.6f, 0.5f, 0.45f, 0.5f, 0.45f, 0.35f };
    juce::StringArray divs; for (int i = 0; i < kNumDivisions; ++i) divs.add (divisionName (i));
    for (int l = 0; l < kNumLanes; ++l)
    {
        const juce::String n = juce::String (voiceName (l)) + " ";
        addB (ids::lane (l, "on"), n + "On", true);
        addI (ids::lane (l, "steps"), n + "Steps", 1, kMaxSteps, 16);
        addC (ids::lane (l, "div"), n + "Division", divs, 3);
        addC (ids::lane (l, "mode"), n + "Mode", { "Polymeter", "Polyrhythm" }, 0);
        addC (ids::lane (l, "span"), n + "Span", { "1 Bar", "2 Bars", "4 Bars" }, 0);
        addB (ids::lane (l, "euclid"), n + "Euclid", false);
        addI (ids::lane (l, "hits"), n + "Hits", 0, kMaxSteps, 4);
        addI (ids::lane (l, "rot"), n + "Rotate", 0, kMaxSteps - 1, 0);
        addF (ids::lane (l, "prob"), n + "Probability", Range (0.0f, 1.0f, 0.001f), 1.0f, fmtPct);
        addF (ids::lane (l, "swing"), n + "Swing", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
        addF (ids::lane (l, "nudge"), n + "Nudge", Range (-30.0f, 30.0f, 0.1f), 0.0f, fmtMs);
        addF (ids::lane (l, "ratchet"), n + "Ratchet", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
        addF (ids::lane (l, "gate"), n + "Gate", Range (0.05f, 1.0f, 0.001f), 0.5f, fmtPct);
        addF (ids::lane (l, "vel"), n + "Velocity", Range (0.0f, 1.0f, 0.001f), 0.9f, fmtPct);
        addF (ids::lane (l, "accent"), n + "Accent", Range (0.0f, 1.0f, 0.001f), 0.3f, fmtPct);
        addI (ids::lane (l, "note"), n + "MIDI Note", 0, 127, defaultNotes[l]);
        addI (ids::lane (l, "chan"), n + "MIDI Channel", 1, 16, 10);
        addF (ids::lane (l, "level"), n + "Level", Range (0.0f, 1.0f, 0.001f), defaultLevel[l], fmtPct);
        addF (ids::lane (l, "pan"), n + "Pan", Range (-1.0f, 1.0f, 0.001f), 0.0f, fmtBipolar);
        addF (ids::lane (l, "tune"), n + "Tune", Range (-24.0f, 24.0f, 0.1f), 0.0f, fmtSemi);
        addF (ids::lane (l, "decay"), n + "Decay", Range (0.0f, 1.0f, 0.001f), l == 0 ? 0.45f : (l == 3 ? 0.2f : 0.4f), fmtPct);
        addF (ids::lane (l, "tone"), n + "Tone", Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
        addF (ids::lane (l, "c1"), n + juce::String (voiceCtlName (l, 0)), Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
        addF (ids::lane (l, "c2"), n + juce::String (voiceCtlName (l, 1)), Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
        addF (ids::lane (l, "c3"), n + juce::String (voiceCtlName (l, 2)), Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
    }
    (void) fmtNote;
    return layout;
}

std::atomic<float>* PulseParamCache::get (juce::AudioProcessorValueTreeState& s, const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p); return p; }

PulseParamCache::PulseParamCache (juce::AudioProcessorValueTreeState& s)
{
    run = get (s, "run"); hostSync = get (s, "hostSync"); swing = get (s, "swing"); humTime = get (s, "humTime"); humVel = get (s, "humVel"); density = get (s, "density");
    fill = get (s, "fill"); pattern = get (s, "pattern"); master = get (s, "master"); busDrive = get (s, "busDrive"); busComp = get (s, "busComp"); busThresh = get (s, "busThresh"); busLimit = get (s, "busLimit");
    for (int l = 0; l < kNumLanes; ++l)
    {
        auto& L = lanes[l];
        L.on = get (s, ids::lane (l, "on")); L.steps = get (s, ids::lane (l, "steps")); L.div = get (s, ids::lane (l, "div")); L.mode = get (s, ids::lane (l, "mode")); L.span = get (s, ids::lane (l, "span"));
        L.euclid = get (s, ids::lane (l, "euclid")); L.hits = get (s, ids::lane (l, "hits")); L.rot = get (s, ids::lane (l, "rot")); L.prob = get (s, ids::lane (l, "prob")); L.swing = get (s, ids::lane (l, "swing"));
        L.nudge = get (s, ids::lane (l, "nudge")); L.ratchet = get (s, ids::lane (l, "ratchet")); L.gate = get (s, ids::lane (l, "gate")); L.vel = get (s, ids::lane (l, "vel")); L.accent = get (s, ids::lane (l, "accent"));
        L.note = get (s, ids::lane (l, "note")); L.chan = get (s, ids::lane (l, "chan")); L.level = get (s, ids::lane (l, "level")); L.pan = get (s, ids::lane (l, "pan")); L.tune = get (s, ids::lane (l, "tune"));
        L.decay = get (s, ids::lane (l, "decay")); L.tone = get (s, ids::lane (l, "tone")); L.c1 = get (s, ids::lane (l, "c1")); L.c2 = get (s, ids::lane (l, "c2")); L.c3 = get (s, ids::lane (l, "c3"));
    }
}

static inline float V (std::atomic<float>* p) { return p->load (std::memory_order_relaxed); }
static inline bool B (std::atomic<float>* p) { return V (p) >= 0.5f; }
static inline int I (std::atomic<float>* p) { return juce::roundToInt (V (p)); }

void PulseParamCache::fillGlobal (GlobalParams& g, bool& r, bool& hs, int& pat, float& m, float& bd, bool& bc, float& bt, bool& bl) const
{
    g.swing = V (swing); g.humanizeTime = V (humTime); g.humanizeVel = V (humVel); g.density = V (density); g.fill = B (fill);
    r = B (run); hs = B (hostSync); pat = I (pattern); m = V (master); bd = V (busDrive); bc = B (busComp); bt = V (busThresh); bl = B (busLimit);
}
void PulseParamCache::fillLane (int l, LaneParams& lp, VoiceParams& vp) const
{
    const auto& L = lanes[l];
    lp.enabled = B (L.on); lp.steps = I (L.steps); lp.division = I (L.div); lp.mode = (LaneMode) I (L.mode); lp.spanBars = 1 << I (L.span);
    lp.euclid = B (L.euclid); lp.hits = I (L.hits); lp.rotation = I (L.rot); lp.probability = V (L.prob); lp.swing = V (L.swing); lp.nudgeMs = V (L.nudge);
    lp.ratchet = V (L.ratchet); lp.gate = V (L.gate); lp.velocity = V (L.vel); lp.accent = V (L.accent); lp.midiNote = I (L.note); lp.midiChannel = I (L.chan);
    vp.level = V (L.level); vp.pan = V (L.pan); vp.tune = V (L.tune); vp.decay = V (L.decay); vp.tone = V (L.tone); vp.ctl[0] = V (L.c1); vp.ctl[1] = V (L.c2); vp.ctl[2] = V (L.c3);
}

} // namespace pulse
