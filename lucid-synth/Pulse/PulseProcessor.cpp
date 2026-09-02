#include "PulseProcessor.h"
#include "PulseEditor.h"

namespace pulse {

#ifdef PULSE_MIDI_ONLY
bool PulseProcessor::isMidiOnlyBuild() { return true; }
#else
bool PulseProcessor::isMidiOnlyBuild() { return false; }
#endif

const juce::String PulseProcessor::getName() const { return isMidiOnlyBuild() ? "LUCID Pulse MIDI" : "LUCID Pulse"; }
bool PulseProcessor::producesMidi() const { return isMidiOnlyBuild(); }
bool PulseProcessor::isMidiEffect() const { return isMidiOnlyBuild(); }

PulseProcessor::PulseProcessor()
    : AudioProcessor (
#ifdef PULSE_MIDI_ONLY
          BusesProperties()
#else
          BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ), apvts (*this, nullptr, "PULSE", createPulseLayout()), cache (apvts)
{
    for (auto& p : steps) for (auto& l : p) for (auto& s : l) s.store (0);
    events.reserve (512);
    loadPreset (0);
}

bool PulseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#ifdef PULSE_MIDI_ONLY
    juce::ignoreUnused (layouts); return true;
#else
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
#endif
}

void PulseProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    seq.prepare ((float) sr);
    kit.prepare ((float) sr);
    bus.prepare ((float) sr);
    renderBuffer.setSize (2, samplesPerBlock);
    busParams = lucid::SynthParams();
    busParams.fx.satType = 0; busParams.fx.satMix = 1.0f; busParams.fx.compRatio = 4.0f; busParams.fx.compAttack = 0.005f; busParams.fx.compRelease = 0.12f; busParams.fx.compMakeup = 3.0f;
    busParams.fx.limiterCeiling = -0.3f;
    setLatencySamples (isMidiOnlyBuild() ? 0 : bus.getLatency (busParams));
}

void PulseProcessor::syncPatternToSequencer()
{
    const int p = currentPattern.load();
    auto& pat = seq.getPattern();
    for (int l = 0; l < kNumLanes; ++l)
        for (int i = 0; i < kMaxSteps; ++i)
            pat.steps[(size_t) l][(size_t) i] = steps[(size_t) p][(size_t) l][(size_t) i].load (std::memory_order_relaxed);
}

std::array<uint8_t, kMaxSteps> PulseProcessor::displaySteps (int lane) const
{
    LaneParams lp; VoiceParams vp; cache.fillLane (lane, lp, vp);
    if (lp.euclid) return euclidean (lp.steps, lp.hits, lp.rotation);
    std::array<uint8_t, kMaxSteps> out {};
    const int p = currentPattern.load();
    for (int i = 0; i < kMaxSteps; ++i) out[(size_t) i] = steps[(size_t) p][(size_t) lane][(size_t) i].load (std::memory_order_relaxed);
    return out;
}

void PulseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // ---- parameters
    GlobalParams g; bool run, hostSync; int pattern; float master, busDrive, busThresh; bool busComp, busLimit;
    cache.fillGlobal (g, run, hostSync, pattern, master, busDrive, busComp, busThresh, busLimit);
    currentPattern.store (pattern);
    for (int l = 0; l < kNumLanes; ++l) { LaneParams lp; cache.fillLane (l, lp, voiceParams[(size_t) l]); seq.setLaneParams (l, lp); }
    syncPatternToSequencer();

    // ---- transport
    double bpm = 120.0, hostBeat = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto p = pos->getPpqPosition()) hostBeat = *p;
            playing = pos->getIsPlaying();
            if (auto ts = pos->getTimeSignature()) g.beatsPerBar = juce::jlimit (1, 16, ts->numerator * 4 / juce::jmax (1, ts->denominator));
        }
    const bool standalone = wrapperType == wrapperType_Standalone;
    const bool useHost = hostSync && ! standalone;
    const bool active = run && (! useHost || playing);
    seq.setGlobal (g);
    seq.setRunning (active);
    if (useHost && ! playing) seq.resetClock();
    const double beat = seq.process (useHost, hostBeat, bpm, numSamples, events);
    transportRunning.store (active);
    displayBeat.store (beat);
    for (int l = 0; l < kNumLanes; ++l) currentStep[l].store (active ? seq.currentStep (l, beat) : -1);

    // ---- incoming MIDI (instrument: play the drums from a keyboard, MIDI FX: pass through)
    midiOut.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (isMidiOnlyBuild()) { midiOut.addEvent (m, meta.samplePosition); continue; }
        if (m.isNoteOn())
        {
            const int lane = m.getNoteNumber() - 36;
            if (lane >= 0 && lane < kNumLanes) events.push_back ({ lane, meta.samplePosition, m.getFloatVelocity(), true, m.getNoteNumber(), 10, 0 });
        }
    }
    if (! events.empty() && ! isMidiOnlyBuild())
        std::stable_sort (events.begin(), events.end(), [] (const SeqEvent& a, const SeqEvent& b) { return a.sampleOffset < b.sampleOffset; });

#ifdef PULSE_MIDI_ONLY
    for (const auto& e : events)
    {
        if (e.noteOn) { midiOut.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, e.velocity), e.sampleOffset); laneFlash[e.lane].store (4); }
        else midiOut.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), e.sampleOffset);
    }
    midi.swapWith (midiOut);
    for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, numSamples);
#else
    if (renderBuffer.getNumSamples() < numSamples) renderBuffer.setSize (2, numSamples, false, false, true);
    float* L = renderBuffer.getWritePointer (0); float* R = renderBuffer.getWritePointer (1);
    size_t ei = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        while (ei < events.size() && events[ei].sampleOffset <= i)
        {
            const auto& e = events[ei++];
            if (e.noteOn) { kit.trigger (e.lane, voiceParams[(size_t) e.lane], e.velocity); laneFlash[e.lane].store (4); }
        }
        kit.process (L[i], R[i]);
    }
    busParams.masterGain = master;
    busParams.fx.satOn = busDrive > 0.001f; busParams.fx.satDrive = busDrive;
    busParams.fx.compOn = busComp; busParams.fx.compThreshold = busThresh;
    busParams.fx.limiterOn = busLimit; busParams.bpm = bpm;
    bus.process (L, R, numSamples, busParams, lucid::FxModulation());
    if (getLatencySamples() != bus.getLatency (busParams)) setLatencySamples (bus.getLatency (busParams));
    const int outCh = getTotalNumOutputChannels();
    if (outCh >= 2) { buffer.copyFrom (0, 0, L, numSamples); buffer.copyFrom (1, 0, R, numSamples); for (int c = 2; c < outCh; ++c) buffer.clear (c, 0, numSamples); }
    else if (outCh == 1) { auto* m = buffer.getWritePointer (0); for (int i = 0; i < numSamples; ++i) m[i] = 0.5f * (L[i] + R[i]); }
    float pl = 0, pr = 0; for (int i = 0; i < numSamples; ++i) { pl = std::max (pl, std::fabs (L[i])); pr = std::max (pr, std::fabs (R[i])); }
    peakL.store (std::max (pl, peakL.load() * 0.85f)); peakR.store (std::max (pr, peakR.load() * 0.85f));
    midi.clear();
#endif
}

// ------------------------------------------------------------------------------ patterns & presets
void PulseProcessor::clearPattern (int p) { for (auto& l : steps[(size_t) p]) for (auto& s : l) s.store (0); }
void PulseProcessor::copyPattern (int from, int to)
{
    for (int l = 0; l < kNumLanes; ++l) for (int i = 0; i < kMaxSteps; ++i) steps[(size_t) to][(size_t) l][(size_t) i].store (steps[(size_t) from][(size_t) l][(size_t) i].load());
}

void PulseProcessor::generate (int style, uint32_t seed)
{
    std::array<GeneratedLane, kNumLanes> g; generateStyle (style, seed, g);
    const int p = currentPattern.load();
    auto set = [this] (const juce::String& id, float v) { if (auto* par = apvts.getParameter (id)) par->setValueNotifyingHost (par->convertTo0to1 (v)); };
    for (int l = 0; l < kNumLanes; ++l)
    {
        for (int i = 0; i < kMaxSteps; ++i) steps[(size_t) p][(size_t) l][(size_t) i].store (g[(size_t) l].steps[(size_t) i]);
        set (ids::lane (l, "steps"), (float) g[(size_t) l].numSteps);
        set (ids::lane (l, "div"), (float) g[(size_t) l].division);
        set (ids::lane (l, "mode"), (float) (int) g[(size_t) l].mode);
        set (ids::lane (l, "span"), g[(size_t) l].spanBars >= 4 ? 2.0f : (g[(size_t) l].spanBars == 2 ? 1.0f : 0.0f));
        set (ids::lane (l, "euclid"), 0.0f);
        set (ids::lane (l, "prob"), g[(size_t) l].probability);
        set (ids::lane (l, "ratchet"), g[(size_t) l].ratchet);
        set (ids::lane (l, "on"), 1.0f);
    }
}

const std::vector<PulsePreset>& pulsePresets()
{
    using V = std::pair<const char*, float>;
    static const std::vector<PulsePreset> presets = {
        { "Minimal 128", 0, 11u, 0.10f, { V { "l1_decay", 0.42f }, V { "l1_c1", 0.55f }, V { "l1_c3", 0.25f }, V { "l4_decay", 0.12f }, V { "l4_c2", 0.6f }, V { "l5_decay", 0.35f }, V { "l6_tune", 5.0f }, V { "l6_c1", 0.7f }, V { "l8_c2", 0.2f }, V { "busDrive", 0.2f } } },
        { "Deep House", 1, 23u, 0.22f, { V { "l1_decay", 0.55f }, V { "l1_tone", 0.35f }, V { "l3_c1", 0.7f }, V { "l3_c2", 0.6f }, V { "l4_decay", 0.18f }, V { "l4_c2", 0.45f }, V { "l5_decay", 0.5f }, V { "l6_tune", -3.0f }, V { "l7_c1", 0.6f }, V { "busThresh", -14.0f } } },
        { "Dub Techno", 2, 37u, 0.08f, { V { "l1_decay", 0.5f }, V { "l1_c3", 0.4f }, V { "l3_c2", 0.8f }, V { "l3_decay", 0.6f }, V { "l4_decay", 0.1f }, V { "l6_decay", 0.6f }, V { "l6_c2", 0.7f }, V { "l8_decay", 0.7f }, V { "l8_c2", 0.85f }, V { "l8_c3", 0.6f }, V { "busDrive", 0.35f } } },
        { "Polyrhythm 5:4", 3, 5u, 0.0f, { V { "l1_c1", 0.7f }, V { "l3_c1", 0.4f }, V { "l6_tune", 7.0f }, V { "l6_c1", 0.85f }, V { "l7_c1", 0.5f }, V { "l8_c1", 0.8f }, V { "l8_decay", 0.3f } } },
        { "Broken UK", 4, 71u, 0.3f, { V { "l1_decay", 0.35f }, V { "l1_c2", 0.7f }, V { "l2_c1", 0.7f }, V { "l2_c3", 0.7f }, V { "l4_c2", 0.7f }, V { "l6_c1", 0.9f }, V { "l6_tune", 12.0f }, V { "humTime", 0.25f }, V { "busDrive", 0.3f } } },
        { "Afro 12/8", 5, 99u, 0.0f, { V { "l1_decay", 0.3f }, V { "l1_tone", 0.6f }, V { "l6_tune", 4.0f }, V { "l6_c1", 0.6f }, V { "l7_c1", 0.7f }, V { "l7_tune", 3.0f }, V { "l4_c1", 0.7f }, V { "humVel", 0.3f } } },
    };
    return presets;
}

void PulseProcessor::loadInit()
{
    for (auto* p : getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (int p = 0; p < kNumPatterns; ++p) clearPattern (p);
    presetName = "Init"; currentPreset = 0;
    if (onPresetChanged) onPresetChanged();
}

void PulseProcessor::loadPreset (int index)
{
    const auto& all = pulsePresets();
    if (! juce::isPositiveAndBelow (index, (int) all.size())) return;
    const auto& pr = all[(size_t) index];
    for (auto* p : getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (int p = 0; p < kNumPatterns; ++p) clearPattern (p);
    currentPattern.store (0);
    if (auto* par = apvts.getParameter ("pattern")) par->setValueNotifyingHost (0.0f);
    // pattern A from the style, B..D as variations with different seeds
    for (int p = 0; p < kNumPatterns; ++p) { currentPattern.store (p); generate (pr.style, pr.seed + (uint32_t) p * 17u); }
    currentPattern.store (0);
    if (auto* par = apvts.getParameter ("swing")) par->setValueNotifyingHost (par->convertTo0to1 (pr.swing));
    for (const auto& v : pr.values)
        if (auto* par = apvts.getParameter (v.first)) par->setValueNotifyingHost (par->convertTo0to1 (v.second)); else jassertfalse;
    presetName = pr.name; currentPreset = index;
    if (onPresetChanged) onPresetChanged();
}

void PulseProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    for (int p = 0; p < kNumPatterns; ++p)
        for (int l = 0; l < kNumLanes; ++l)
        {
            juce::String s; for (int i = 0; i < kMaxSteps; ++i) s += juce::String ((int) getStep (p, l, i));
            state.setProperty ("pat" + juce::String (p) + "_l" + juce::String (l), s, nullptr);
        }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void PulseProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        auto tree = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (tree);
        for (int p = 0; p < kNumPatterns; ++p)
            for (int l = 0; l < kNumLanes; ++l)
            {
                const auto s = tree.getProperty ("pat" + juce::String (p) + "_l" + juce::String (l)).toString();
                for (int i = 0; i < kMaxSteps; ++i) steps[(size_t) p][(size_t) l][(size_t) i].store (i < s.length() ? (uint8_t) juce::jlimit (0, 4, s[i] - '0') : (uint8_t) 0);
            }
        presetName = tree.getProperty ("presetName", "Init").toString();
        if (onPresetChanged) onPresetChanged();
    }
}

juce::AudioProcessorEditor* PulseProcessor::createEditor() { return new PulseEditor (*this); }

} // namespace pulse

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new pulse::PulseProcessor(); }
