// LUCID PULSE - parameter layout and conversion into sequencer / drum parameters
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Sequencer.h"
#include "dsp/DrumKit.h"

namespace pulse {

namespace ids
{
    inline juce::String lane (int l, const char* p) { return "l" + juce::String (l + 1) + "_" + p; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createPulseLayout();

class PulseParamCache
{
public:
    explicit PulseParamCache (juce::AudioProcessorValueTreeState& apvts);
    void fillGlobal (GlobalParams& g, bool& run, bool& hostSync, int& pattern, float& master, float& busDrive, bool& busComp, float& busThresh, bool& busLimit) const;
    void fillLane (int l, LaneParams& lp, VoiceParams& vp) const;
private:
    std::atomic<float>* get (juce::AudioProcessorValueTreeState& s, const juce::String& id);
    std::atomic<float> *run, *hostSync, *swing, *humTime, *humVel, *density, *fill, *pattern, *master, *busDrive, *busComp, *busThresh, *busLimit;
    struct Lane { std::atomic<float> *on, *steps, *div, *mode, *span, *euclid, *hits, *rot, *prob, *swing, *nudge, *ratchet, *gate, *vel, *accent, *note, *chan, *level, *pan, *tune, *decay, *tone, *c1, *c2, *c3; } lanes[kNumLanes];
};

} // namespace pulse
