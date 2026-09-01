#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace lucid {

LucidAudioProcessor::LucidAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LUCID", createParameterLayout()),
      presets (apvts),
      cache (apvts)
{
    cache.fill (params);
    engine.setParams (params);
}

bool LucidAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void LucidAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare ((float) sampleRate, samplesPerBlock);
    renderBuffer.setSize (2, samplesPerBlock);
    cache.fill (params);
    engine.setParams (params);
    setLatencySamples (engine.getLatency());
}

void LucidAudioProcessor::setCurrentProgram (int index)
{
    if (index != presets.getCurrentIndex()) presets.loadPreset (index);
}

void LucidAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())            engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())      engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())   engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 1)       engine.setModWheel (m.getControllerValue() / 127.0f);
        else if (cc == 64) engine.setSustain (m.getControllerValue() >= 64);
        else if (cc == 120 || cc == 123) engine.allNotesOff();
    }
    else if (m.isChannelPressure()) engine.setAftertouch (m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch())      engine.setAftertouch (m.getAfterTouchValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void LucidAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // parameters -> engine snapshot
    cache.fill (params);
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) params.bpm = juce::jlimit (20.0, 400.0, *bpm);
    }
    engine.setParams (params);
    if (getLatencySamples() != engine.getLatency()) setLatencySamples (engine.getLatency());

    if (renderBuffer.getNumSamples() < numSamples) renderBuffer.setSize (2, numSamples, false, false, true);
    float* L = renderBuffer.getWritePointer (0);
    float* R = renderBuffer.getWritePointer (1);
    juce::FloatVectorOperations::clear (L, numSamples);
    juce::FloatVectorOperations::clear (R, numSamples);

    // sample-accurate MIDI: render voices in sub-blocks between events
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        if (at > pos) { engine.renderVoices (L + pos, R + pos, at - pos); pos = at; }
        handleMidi (meta.getMessage());
    }
    if (pos < numSamples) engine.renderVoices (L + pos, R + pos, numSamples - pos);
    engine.renderEffects (L, R, numSamples);

    // output
    const int outCh = getTotalNumOutputChannels();
    if (outCh >= 2)
    {
        buffer.copyFrom (0, 0, L, numSamples);
        buffer.copyFrom (1, 0, R, numSamples);
        for (int c = 2; c < outCh; ++c) buffer.clear (c, 0, numSamples);
    }
    else if (outCh == 1)
    {
        auto* m = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i) m[i] = 0.5f * (L[i] + R[i]);
    }

    // UI feeds
    scopeFeed.push (L, R, numSamples);
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < numSamples; ++i) { pl = std::max (pl, std::fabs (L[i])); pr = std::max (pr, std::fabs (R[i])); }
    peakL.store (std::max (pl, peakL.load() * 0.85f)); peakR.store (std::max (pr, peakR.load() * 0.85f));
    activeVoices.store (engine.getActiveVoiceCount());
    if (const auto* v = engine.newestVoice())
    {
        for (int i = 0; i < kNumEnvs; ++i) envDisplay[i].store (v->getEnvLevel (i));
        for (int i = 0; i < kNumLfos; ++i) lfoDisplay[i].store (params.lfo[(size_t) i].mono ? engine.getGlobalLfoValue (i) : v->getLastLfo (i));
    }
    else
    {
        for (int i = 0; i < kNumEnvs; ++i) envDisplay[i].store (0.0f);
        for (int i = 0; i < kNumLfos; ++i) lfoDisplay[i].store (engine.getGlobalLfoValue (i));
    }
    limiterGr.store (engine.getEffects().getLimiter().getGainReductionDb());
    compGr.store (engine.getEffects().getCompressor().getGainReductionDb());
}

juce::AudioProcessorEditor* LucidAudioProcessor::createEditor() { return new LucidAudioProcessorEditor (*this); }

void LucidAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void LucidAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            presets.stateRestored (tree.getProperty ("presetName").toString());
        }
    }
}

} // namespace lucid

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new lucid::LucidAudioProcessor(); }
