#include "Parameters.h"
#include "dsp/Wavetable.h"

namespace lucid {

using APF = juce::AudioParameterFloat;
using API = juce::AudioParameterInt;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using Range = juce::NormalisableRange<float>;

juce::StringArray modSourceNames()
{
    return { "-", "Env 1 (Amp)", "Env 2 (Filter)", "Env 3 (Mod)", "LFO 1", "LFO 2", "LFO 3", "Velocity", "Mod Wheel", "Aftertouch",
             "Key Track", "Pitch Bend", "Random", "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
}
juce::StringArray modDestNames()
{
    return { "-", "Osc A Pitch", "Osc B Pitch", "Pitch (All)", "Osc A Morph", "Osc B Morph", "Osc A Level", "Osc B Level",
             "Osc A PW", "Osc B PW", "Osc A Pan", "Osc B Pan", "FM Amount", "Osc A Detune", "Osc B Detune",
             "Sub Level", "Noise Level",
             "Filter 1 Cutoff", "Filter 2 Cutoff", "Cutoff (All)", "Filter 1 Reso", "Filter 2 Reso", "Filter Drive",
             "Amp Level", "Amp Pan",
             "LFO 1 Rate", "LFO 2 Rate", "LFO 3 Rate", "Env 2 Amount",
             "Chorus Mix", "Delay Mix", "Delay Feedback", "Reverb Mix", "Reverb Size" };
}
juce::StringArray wavetableNames()
{
    juce::StringArray s; for (int i = 0; i < WavetableBank::kNumTables; ++i) s.add (WavetableBank::tableName (i)); return s;
}
juce::StringArray syncDivisionNames()
{
    return { "8 Bars", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/4 D", "1/8", "1/8 T", "1/16", "1/16 T", "1/32", "1/32 T" };
}
juce::StringArray filterTypeNames() { return { "LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "Notch", "Ladder" }; }
juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S & H", "Smooth Rnd" }; }

static Range logRange (float lo, float hi)
{
    return Range (lo, hi,
        [] (float s, float e, float n) { return s * std::pow (e / s, n); },
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
        [] (float, float, float v) { return v; });
}
static Range timeRange (float lo, float hi)
{
    Range r (lo, hi, 0.0001f);
    r.setSkewForCentre (lo > 0.0f ? std::sqrt (lo * hi) : hi * 0.12f); // ranges starting at 0 need an explicit centre
    return r;
}

static juce::String fmtHz (float v, int)
{
    if (v >= 10000.0f) return juce::String (v / 1000.0f, 1) + "k";
    if (v >= 1000.0f)  return juce::String (v / 1000.0f, 2) + "k";
    if (v >= 100.0f)   return juce::String (juce::roundToInt (v)) + "Hz";
    return juce::String (v, v < 10.0f ? 2 : 1) + "Hz";
}
static juce::String fmtMs (float v, int) { return v < 1.0f ? juce::String (v * 1000.0f, v < 0.01f ? 1 : 0) + " ms" : juce::String (v, 2) + " s"; }
static juce::String fmtMsPlain (float v, int) { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " ms" : juce::String (v / 1000.0f, 2) + " s"; }
static juce::String fmtRatio (float v, int) { return juce::String (v, 1) + ":1"; }
static juce::String fmtPlain2 (float v, int) { return juce::String (v, 2); }
static juce::String fmtCents (float v, int) { return (v >= 0 ? "+" : "") + juce::String (v, 0) + " ct"; }
static juce::String fmtPct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
static juce::String fmtDb (float v, int) { return juce::String (v, 1) + " dB"; }
static juce::String fmtBipolar (float v, int) { return (v >= 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pid = [] (const juce::String& id) { return juce::ParameterID { id, 1 }; };

    auto addF = [&] (const juce::String& id, const juce::String& name, Range range, float def, juce::String (*fmt) (float, int) = nullptr)
    {
        auto attr = juce::AudioParameterFloatAttributes();
        if (fmt != nullptr) attr = attr.withStringFromValueFunction (fmt);
        layout.add (std::make_unique<APF> (pid (id), name, range, def, attr));
    };
    auto addI = [&] (const juce::String& id, const juce::String& name, int lo, int hi, int def)
    { layout.add (std::make_unique<API> (pid (id), name, lo, hi, def)); };
    auto addB = [&] (const juce::String& id, const juce::String& name, bool def)
    { layout.add (std::make_unique<APB> (pid (id), name, def)); };
    auto addC = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& choices, int def)
    { layout.add (std::make_unique<APC> (pid (id), name, choices, def)); };

    // ---- global
    addF ("master", "Master", Range (-60.0f, 12.0f, 0.1f), 0.0f, fmtDb);
    addI ("voices", "Voices", 1, kMaxVoices, 16);
    addC ("voiceMode", "Voice Mode", { "Poly", "Mono", "Legato" }, 0);
    addF ("glide", "Glide", timeRange (0.0f, 2.0f), 0.0f, fmtMs);
    addI ("bendRange", "Bend Range", 0, 24, 2);
    addF ("velAmp", "Velocity > Amp", Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
    addF ("pan", "Pan", Range (-1.0f, 1.0f, 0.001f), 0.0f, fmtBipolar);
    for (int i = 0; i < kNumMacros; ++i) addF (ids::macro (i), "Macro " + juce::String (i + 1), Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);

    // ---- oscillators
    for (int i = 0; i < kNumOscs; ++i)
    {
        const juce::String n = i == 0 ? "A " : "B ";
        addB (ids::osc (i, "on"), n + "On", i == 0);
        addC (ids::osc (i, "engine"), n + "Engine", { "Wavetable", "Analog" }, 0);
        addC (ids::osc (i, "table"), n + "Wavetable", wavetableNames(), i == 0 ? 1 : 2);
        addF (ids::osc (i, "morph"), n + "Morph", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
        addC (ids::osc (i, "shape"), n + "Shape", { "Saw", "Square", "Triangle", "Sine" }, 0);
        addF (ids::osc (i, "pw"), n + "Pulse Width", Range (0.05f, 0.95f, 0.001f), 0.5f, fmtPct);
        addI (ids::osc (i, "oct"), n + "Octave", -3, 3, 0);
        addI (ids::osc (i, "semi"), n + "Semi", -12, 12, 0);
        addF (ids::osc (i, "fine"), n + "Fine", Range (-100.0f, 100.0f, 0.1f), 0.0f, fmtCents);
        addF (ids::osc (i, "level"), n + "Level", Range (0.0f, 1.0f, 0.001f), i == 0 ? 0.8f : 0.6f, fmtPct);
        addF (ids::osc (i, "pan"), n + "Pan", Range (-1.0f, 1.0f, 0.001f), 0.0f, fmtBipolar);
        addI (ids::osc (i, "unison"), n + "Unison", 1, kMaxUnison, 1);
        addF (ids::osc (i, "detune"), n + "Detune", Range (0.0f, 1.0f, 0.001f), 0.25f, fmtPct);
        addF (ids::osc (i, "spread"), n + "Spread", Range (0.0f, 1.0f, 0.001f), 0.6f, fmtPct);
        addF (ids::osc (i, "blend"), n + "Blend", Range (0.0f, 1.0f, 0.001f), 0.75f, fmtPct);
        addF (ids::osc (i, "phase"), n + "Phase", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
        addB (ids::osc (i, "retrig"), n + "Retrigger", false);
        addF (ids::osc (i, "drift"), n + "Drift", Range (0.0f, 1.0f, 0.001f), 0.15f, fmtPct);
    }
    addF ("fm", "FM B > A", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
    addF ("ring", "Ring Mod", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
    addF ("sub_level", "Sub Level", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
    addC ("sub_oct", "Sub Octave", { "-1 Oct", "-2 Oct" }, 0);
    addC ("sub_shape", "Sub Shape", { "Sine", "Triangle", "Square" }, 0);
    addF ("noise_level", "Noise Level", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
    addF ("noise_color", "Noise Colour", Range (-1.0f, 1.0f, 0.001f), 0.0f, fmtBipolar);

    // ---- filters
    for (int i = 0; i < kNumFilters; ++i)
    {
        const juce::String n = "Filter " + juce::String (i + 1) + " ";
        addB (ids::filt (i, "on"), n + "On", i == 0);
        addC (ids::filt (i, "type"), n + "Type", filterTypeNames(), i == 0 ? 1 : 2);
        addF (ids::filt (i, "cutoff"), n + "Cutoff", logRange (20.0f, 20000.0f), i == 0 ? 8000.0f : 200.0f, fmtHz);
        addF (ids::filt (i, "res"), n + "Resonance", Range (0.0f, 1.0f, 0.001f), 0.15f, fmtPct);
        addF (ids::filt (i, "drive"), n + "Drive", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
        addF (ids::filt (i, "key"), n + "Key Track", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
        addF (ids::filt (i, "env"), n + "Env Amount", Range (-1.0f, 1.0f, 0.001f), i == 0 ? 0.4f : 0.0f, fmtBipolar);
    }
    addC ("routing", "Filter Routing", { "Serial", "Parallel", "Split" }, 0);
    addF ("filterMix", "Filter Mix", Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);

    // ---- envelopes
    static const char* envNames[kNumEnvs] = { "Amp Env ", "Filter Env ", "Mod Env " };
    for (int i = 0; i < kNumEnvs; ++i)
    {
        const juce::String n = envNames[i];
        addF (ids::env (i, "a"), n + "Attack", timeRange (0.0005f, 10.0f), i == 0 ? 0.005f : 0.01f, fmtMs);
        addF (ids::env (i, "d"), n + "Decay", timeRange (0.001f, 10.0f), i == 0 ? 0.3f : 0.5f, fmtMs);
        addF (ids::env (i, "s"), n + "Sustain", Range (0.0f, 1.0f, 0.001f), i == 0 ? 0.8f : 0.2f, fmtPct);
        addF (ids::env (i, "r"), n + "Release", timeRange (0.001f, 15.0f), i == 0 ? 0.4f : 0.5f, fmtMs);
        addF (ids::env (i, "curve"), n + "Curve", Range (0.0f, 1.0f, 0.001f), 0.6f, fmtPct);
        addF (ids::env (i, "vel"), n + "Velocity", Range (0.0f, 1.0f, 0.001f), i == 0 ? 0.0f : 0.3f, fmtPct);
    }

    // ---- LFOs
    for (int i = 0; i < kNumLfos; ++i)
    {
        const juce::String n = "LFO " + juce::String (i + 1) + " ";
        addC (ids::lfo (i, "shape"), n + "Shape", lfoShapeNames(), 0);
        addF (ids::lfo (i, "rate"), n + "Rate", logRange (0.01f, 50.0f), 2.0f, fmtHz);
        addB (ids::lfo (i, "sync"), n + "Sync", false);
        addC (ids::lfo (i, "div"), n + "Division", syncDivisionNames(), 7);
        addF (ids::lfo (i, "phase"), n + "Phase", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
        addF (ids::lfo (i, "fade"), n + "Fade In", timeRange (0.0f, 5.0f), 0.0f, fmtMs);
        addB (ids::lfo (i, "mono"), n + "Mono", false);
        addF (ids::lfo (i, "smooth"), n + "Smooth", Range (0.0f, 1.0f, 0.001f), 0.0f, fmtPct);
    }

    // ---- modulation matrix
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const juce::String n = "Mod " + juce::String (i + 1) + " ";
        addC (ids::mod (i, "src"), n + "Source", modSourceNames(), 0);
        addC (ids::mod (i, "dst"), n + "Destination", modDestNames(), 0);
        addF (ids::mod (i, "amt"), n + "Amount", Range (-1.0f, 1.0f, 0.001f), 0.0f, fmtBipolar);
    }

    // ---- effects
    addB ("sat_on", "Saturator On", false);
    addC ("sat_type", "Saturator Type", { "Tape", "Tube", "Fold", "Hard" }, 0);
    addF ("sat_drive", "Saturator Drive", Range (0.0f, 1.0f, 0.001f), 0.3f, fmtPct);
    addF ("sat_mix", "Saturator Mix", Range (0.0f, 1.0f, 0.001f), 1.0f, fmtPct);

    addB ("eq_on", "EQ On", false);
    addF ("eq_lowFreq", "EQ Low Freq", logRange (30.0f, 800.0f), 120.0f, fmtHz);
    addF ("eq_lowGain", "EQ Low Gain", Range (-18.0f, 18.0f, 0.1f), 0.0f, fmtDb);
    addF ("eq_midFreq", "EQ Mid Freq", logRange (100.0f, 12000.0f), 1200.0f, fmtHz);
    addF ("eq_midGain", "EQ Mid Gain", Range (-18.0f, 18.0f, 0.1f), 0.0f, fmtDb);
    addF ("eq_midQ", "EQ Mid Q", logRange (0.2f, 8.0f), 1.0f, fmtPlain2);
    addF ("eq_highFreq", "EQ High Freq", logRange (1500.0f, 18000.0f), 8000.0f, fmtHz);
    addF ("eq_highGain", "EQ High Gain", Range (-18.0f, 18.0f, 0.1f), 0.0f, fmtDb);

    addB ("comp_on", "Compressor On", false);
    addF ("comp_thresh", "Comp Threshold", Range (-48.0f, 0.0f, 0.1f), -18.0f, fmtDb);
    addF ("comp_ratio", "Comp Ratio", logRange (1.0f, 20.0f), 3.0f, fmtRatio);
    addF ("comp_attack", "Comp Attack", timeRange (0.0001f, 0.3f), 0.01f, fmtMs);
    addF ("comp_release", "Comp Release", timeRange (0.01f, 2.0f), 0.15f, fmtMs);
    addF ("comp_makeup", "Comp Makeup", Range (0.0f, 24.0f, 0.1f), 0.0f, fmtDb);
    addF ("comp_mix", "Comp Mix", Range (0.0f, 1.0f, 0.001f), 1.0f, fmtPct);

    addB ("cho_on", "Chorus On", false);
    addF ("cho_rate", "Chorus Rate", logRange (0.05f, 8.0f), 0.6f, fmtHz);
    addF ("cho_depth", "Chorus Depth", Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
    addF ("cho_mix", "Chorus Mix", Range (0.0f, 1.0f, 0.001f), 0.35f, fmtPct);
    addF ("cho_width", "Chorus Width", Range (0.0f, 1.0f, 0.001f), 1.0f, fmtPct);

    addB ("dly_on", "Delay On", false);
    addB ("dly_sync", "Delay Sync", true);
    addC ("dly_div", "Delay Division", syncDivisionNames(), 7);
    addF ("dly_time", "Delay Time", logRange (1.0f, 2000.0f), 375.0f, fmtMsPlain);
    addF ("dly_fb", "Delay Feedback", Range (0.0f, 1.0f, 0.001f), 0.4f, fmtPct);
    addF ("dly_mix", "Delay Mix", Range (0.0f, 1.0f, 0.001f), 0.25f, fmtPct);
    addB ("dly_pingpong", "Delay Ping Pong", true);
    addF ("dly_lowcut", "Delay Low Cut", logRange (20.0f, 2000.0f), 150.0f, fmtHz);
    addF ("dly_highcut", "Delay High Cut", logRange (500.0f, 20000.0f), 6000.0f, fmtHz);

    addB ("rev_on", "Reverb On", false);
    addF ("rev_size", "Reverb Size", Range (0.0f, 1.0f, 0.001f), 0.6f, fmtPct);
    addF ("rev_decay", "Reverb Decay", Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
    addF ("rev_damp", "Reverb Damping", Range (0.0f, 1.0f, 0.001f), 0.5f, fmtPct);
    addF ("rev_predelay", "Reverb Pre-Delay", Range (0.0f, 250.0f, 0.1f), 20.0f, fmtMsPlain);
    addF ("rev_mix", "Reverb Mix", Range (0.0f, 1.0f, 0.001f), 0.25f, fmtPct);
    addF ("rev_width", "Reverb Width", Range (0.0f, 1.0f, 0.001f), 1.0f, fmtPct);

    addB ("lim_on", "Limiter On", true);
    addF ("lim_ceiling", "Limiter Ceiling", Range (-12.0f, 0.0f, 0.1f), -0.3f, fmtDb);

    return layout;
}

// ------------------------------------------------------------------------------------------
ParameterCache::ParameterCache (juce::AudioProcessorValueTreeState& apvts) : state (apvts)
{
    master = get ("master"); voices = get ("voices"); voiceMode = get ("voiceMode"); glide = get ("glide");
    bendRange = get ("bendRange"); velAmp = get ("velAmp"); pan = get ("pan");
    for (int i = 0; i < kNumMacros; ++i) macros[i] = get (ids::macro (i));
    for (int i = 0; i < kNumOscs; ++i)
    {
        auto& o = osc[i];
        o.on = get (ids::osc (i, "on")); o.engine = get (ids::osc (i, "engine")); o.table = get (ids::osc (i, "table")); o.morph = get (ids::osc (i, "morph"));
        o.shape = get (ids::osc (i, "shape")); o.pw = get (ids::osc (i, "pw")); o.oct = get (ids::osc (i, "oct")); o.semi = get (ids::osc (i, "semi"));
        o.fine = get (ids::osc (i, "fine")); o.level = get (ids::osc (i, "level")); o.pan = get (ids::osc (i, "pan")); o.unison = get (ids::osc (i, "unison"));
        o.detune = get (ids::osc (i, "detune")); o.spread = get (ids::osc (i, "spread")); o.blend = get (ids::osc (i, "blend")); o.phase = get (ids::osc (i, "phase"));
        o.retrig = get (ids::osc (i, "retrig")); o.drift = get (ids::osc (i, "drift"));
    }
    fm = get ("fm"); ring = get ("ring"); subLevel = get ("sub_level"); subOct = get ("sub_oct"); subShape = get ("sub_shape");
    noiseLevel = get ("noise_level"); noiseColor = get ("noise_color");
    for (int i = 0; i < kNumFilters; ++i)
    {
        auto& f = filt[i];
        f.on = get (ids::filt (i, "on")); f.type = get (ids::filt (i, "type")); f.cutoff = get (ids::filt (i, "cutoff")); f.res = get (ids::filt (i, "res"));
        f.drive = get (ids::filt (i, "drive")); f.key = get (ids::filt (i, "key")); f.env = get (ids::filt (i, "env"));
    }
    routing = get ("routing"); filterMix = get ("filterMix");
    for (int i = 0; i < kNumEnvs; ++i)
    {
        auto& e = env[i];
        e.a = get (ids::env (i, "a")); e.d = get (ids::env (i, "d")); e.s = get (ids::env (i, "s")); e.r = get (ids::env (i, "r")); e.curve = get (ids::env (i, "curve")); e.vel = get (ids::env (i, "vel"));
    }
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto& l = lfo[i];
        l.shape = get (ids::lfo (i, "shape")); l.rate = get (ids::lfo (i, "rate")); l.sync = get (ids::lfo (i, "sync")); l.div = get (ids::lfo (i, "div"));
        l.phase = get (ids::lfo (i, "phase")); l.fade = get (ids::lfo (i, "fade")); l.mono = get (ids::lfo (i, "mono")); l.smooth = get (ids::lfo (i, "smooth"));
    }
    for (int i = 0; i < kNumModSlots; ++i) { mod[i].src = get (ids::mod (i, "src")); mod[i].dst = get (ids::mod (i, "dst")); mod[i].amt = get (ids::mod (i, "amt")); }

    fx.satOn = get ("sat_on"); fx.satType = get ("sat_type"); fx.satDrive = get ("sat_drive"); fx.satMix = get ("sat_mix");
    fx.eqOn = get ("eq_on"); fx.eqLowF = get ("eq_lowFreq"); fx.eqLowG = get ("eq_lowGain"); fx.eqMidF = get ("eq_midFreq"); fx.eqMidG = get ("eq_midGain");
    fx.eqMidQ = get ("eq_midQ"); fx.eqHighF = get ("eq_highFreq"); fx.eqHighG = get ("eq_highGain");
    fx.compOn = get ("comp_on"); fx.compThr = get ("comp_thresh"); fx.compRatio = get ("comp_ratio"); fx.compAtt = get ("comp_attack"); fx.compRel = get ("comp_release");
    fx.compMakeup = get ("comp_makeup"); fx.compMix = get ("comp_mix");
    fx.choOn = get ("cho_on"); fx.choRate = get ("cho_rate"); fx.choDepth = get ("cho_depth"); fx.choMix = get ("cho_mix"); fx.choWidth = get ("cho_width");
    fx.dlyOn = get ("dly_on"); fx.dlySync = get ("dly_sync"); fx.dlyDiv = get ("dly_div"); fx.dlyTime = get ("dly_time"); fx.dlyFb = get ("dly_fb"); fx.dlyMix = get ("dly_mix");
    fx.dlyPp = get ("dly_pingpong"); fx.dlyLow = get ("dly_lowcut"); fx.dlyHigh = get ("dly_highcut");
    fx.revOn = get ("rev_on"); fx.revSize = get ("rev_size"); fx.revDecay = get ("rev_decay"); fx.revDamp = get ("rev_damp"); fx.revPre = get ("rev_predelay");
    fx.revMix = get ("rev_mix"); fx.revWidth = get ("rev_width");
    fx.limOn = get ("lim_on"); fx.limCeil = get ("lim_ceiling");
}

std::atomic<float>* ParameterCache::get (const juce::String& id) const
{
    auto* p = state.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}

static inline float V (std::atomic<float>* p) { return p->load (std::memory_order_relaxed); }
static inline bool B (std::atomic<float>* p) { return V (p) >= 0.5f; }
static inline int I (std::atomic<float>* p) { return juce::roundToInt (V (p)); }

void ParameterCache::fill (SynthParams& p) const
{
    p.masterGain = V (master); p.voices = I (voices); p.voiceMode = (VoiceMode) I (voiceMode); p.glideTime = V (glide);
    p.pitchBendRange = V (bendRange); p.ampVelocity = V (velAmp); p.ampPan = V (pan); p.ampLevel = 1.0f;
    for (int i = 0; i < kNumMacros; ++i) p.macros[(size_t) i] = V (macros[i]);
    for (int i = 0; i < kNumOscs; ++i)
    {
        auto& o = p.osc[(size_t) i]; const auto& c = osc[i];
        o.enabled = B (c.on); o.engine = (OscEngine) I (c.engine); o.table = I (c.table); o.morph = V (c.morph); o.shape = (AnalogShape) I (c.shape);
        o.pulseWidth = V (c.pw); o.octave = I (c.oct); o.semi = I (c.semi); o.fine = V (c.fine); o.level = V (c.level); o.pan = V (c.pan);
        o.unison = I (c.unison); o.detune = V (c.detune); o.spread = V (c.spread); o.blend = V (c.blend); o.phase = V (c.phase); o.retrigger = B (c.retrig); o.drift = V (c.drift);
    }
    p.fmAmount = V (fm); p.ringMod = V (ring); p.subLevel = V (subLevel); p.subOctave = I (subOct) + 1; p.subShape = (SubShape) I (subShape);
    p.noiseLevel = V (noiseLevel); p.noiseColor = V (noiseColor);
    for (int i = 0; i < kNumFilters; ++i)
    {
        auto& f = p.filter[(size_t) i]; const auto& c = filt[i];
        f.enabled = B (c.on); f.type = (FilterType) I (c.type); f.cutoff = V (c.cutoff); f.resonance = V (c.res); f.drive = V (c.drive); f.keyTrack = V (c.key); f.envAmount = V (c.env);
    }
    p.routing = (FilterRouting) I (routing); p.filterMix = V (filterMix);
    for (int i = 0; i < kNumEnvs; ++i)
    {
        auto& e = p.env[(size_t) i]; const auto& c = env[i];
        e.attack = V (c.a); e.decay = V (c.d); e.sustain = V (c.s); e.release = V (c.r); e.curve = V (c.curve); e.velocity = V (c.vel);
    }
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto& l = p.lfo[(size_t) i]; const auto& c = lfo[i];
        l.shape = (LfoShape) I (c.shape); l.rateHz = V (c.rate); l.sync = B (c.sync); l.syncDiv = I (c.div); l.phase = V (c.phase); l.fadeIn = V (c.fade); l.mono = B (c.mono); l.smooth = V (c.smooth);
    }
    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto& m = p.mod[(size_t) i]; m.source = (ModSource) I (mod[i].src); m.dest = (ModDest) I (mod[i].dst); m.amount = V (mod[i].amt);
    }
    auto& x = p.fx;
    x.satOn = B (fx.satOn); x.satType = I (fx.satType); x.satDrive = V (fx.satDrive); x.satMix = V (fx.satMix);
    x.eqOn = B (fx.eqOn); x.eqLowFreq = V (fx.eqLowF); x.eqLowGain = V (fx.eqLowG); x.eqMidFreq = V (fx.eqMidF); x.eqMidGain = V (fx.eqMidG); x.eqMidQ = V (fx.eqMidQ);
    x.eqHighFreq = V (fx.eqHighF); x.eqHighGain = V (fx.eqHighG);
    x.compOn = B (fx.compOn); x.compThreshold = V (fx.compThr); x.compRatio = V (fx.compRatio); x.compAttack = V (fx.compAtt); x.compRelease = V (fx.compRel);
    x.compMakeup = V (fx.compMakeup); x.compMix = V (fx.compMix);
    x.chorusOn = B (fx.choOn); x.chorusRate = V (fx.choRate); x.chorusDepth = V (fx.choDepth); x.chorusMix = V (fx.choMix); x.chorusWidth = V (fx.choWidth);
    x.delayOn = B (fx.dlyOn); x.delaySync = B (fx.dlySync); x.delayDiv = I (fx.dlyDiv); x.delayTimeMs = V (fx.dlyTime); x.delayFeedback = V (fx.dlyFb); x.delayMix = V (fx.dlyMix);
    x.delayPingPong = B (fx.dlyPp); x.delayLowCut = V (fx.dlyLow); x.delayHighCut = V (fx.dlyHigh);
    x.reverbOn = B (fx.revOn); x.reverbSize = V (fx.revSize); x.reverbDecay = V (fx.revDecay); x.reverbDamping = V (fx.revDamp); x.reverbPreDelay = V (fx.revPre);
    x.reverbMix = V (fx.revMix); x.reverbWidth = V (fx.revWidth);
    x.limiterOn = B (fx.limOn); x.limiterCeiling = V (fx.limCeil);
}

} // namespace lucid
