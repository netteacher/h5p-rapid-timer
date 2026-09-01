// LUCID Synth - parameter definitions (JUCE AudioProcessorValueTreeState) and conversion into
// the engine's SynthParams snapshot.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Params.h"
#include <vector>

namespace lucid {

namespace ids
{
    inline juce::String osc (int i, const char* p)   { return juce::String (i == 0 ? "oscA_" : "oscB_") + p; }
    inline juce::String filt (int i, const char* p)  { return "f" + juce::String (i + 1) + "_" + p; }
    inline juce::String env (int i, const char* p)   { return "env" + juce::String (i + 1) + "_" + p; }
    inline juce::String lfo (int i, const char* p)   { return "lfo" + juce::String (i + 1) + "_" + p; }
    inline juce::String mod (int i, const char* p)   { return "mod" + juce::String (i + 1) + "_" + p; }
    inline juce::String macro (int i)                { return "macro" + juce::String (i + 1); }
}

juce::StringArray modSourceNames();
juce::StringArray modDestNames();
juce::StringArray wavetableNames();
juce::StringArray syncDivisionNames();
juce::StringArray filterTypeNames();
juce::StringArray lfoShapeNames();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Cached raw-value pointers for fast, allocation-free access on the audio thread
class ParameterCache
{
public:
    explicit ParameterCache (juce::AudioProcessorValueTreeState& apvts);
    void fill (SynthParams& p) const;

private:
    juce::AudioProcessorValueTreeState& state;
    std::atomic<float>* get (const juce::String& id) const;

    std::atomic<float>* master; std::atomic<float>* voices; std::atomic<float>* voiceMode; std::atomic<float>* glide;
    std::atomic<float>* bendRange; std::atomic<float>* velAmp; std::atomic<float>* pan;
    std::atomic<float>* macros[kNumMacros];

    struct Osc { std::atomic<float> *on, *engine, *table, *morph, *shape, *pw, *oct, *semi, *fine, *level, *pan, *unison, *detune, *spread, *blend, *phase, *retrig, *drift; } osc[kNumOscs];
    std::atomic<float> *fm, *ring, *subLevel, *subOct, *subShape, *noiseLevel, *noiseColor;
    struct Filt { std::atomic<float> *on, *type, *cutoff, *res, *drive, *key, *env; } filt[kNumFilters];
    std::atomic<float> *routing, *filterMix;
    struct Env { std::atomic<float> *a, *d, *s, *r, *curve, *vel; } env[kNumEnvs];
    struct Lfo { std::atomic<float> *shape, *rate, *sync, *div, *phase, *fade, *mono, *smooth; } lfo[kNumLfos];
    struct Mod { std::atomic<float> *src, *dst, *amt; } mod[kNumModSlots];
    struct Fx
    {
        std::atomic<float> *satOn, *satType, *satDrive, *satMix;
        std::atomic<float> *eqOn, *eqLowF, *eqLowG, *eqMidF, *eqMidG, *eqMidQ, *eqHighF, *eqHighG;
        std::atomic<float> *compOn, *compThr, *compRatio, *compAtt, *compRel, *compMakeup, *compMix;
        std::atomic<float> *choOn, *choRate, *choDepth, *choMix, *choWidth;
        std::atomic<float> *dlyOn, *dlySync, *dlyDiv, *dlyTime, *dlyFb, *dlyMix, *dlyPp, *dlyLow, *dlyHigh;
        std::atomic<float> *revOn, *revSize, *revDecay, *revDamp, *revPre, *revMix, *revWidth;
        std::atomic<float> *limOn, *limCeil;
    } fx;
};

} // namespace lucid
