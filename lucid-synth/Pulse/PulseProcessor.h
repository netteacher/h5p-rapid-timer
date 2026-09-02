// LUCID PULSE - AudioProcessor. Built twice: as a drum instrument (sequencer + drum synth) and,
// with PULSE_MIDI_ONLY, as a MIDI effect that drives any instrument in the host.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Sequencer.h"
#include "dsp/DrumKit.h"
#include "../Source/dsp/Effects.h"
#include "PulseParameters.h"

namespace pulse {

struct PulsePreset { const char* name; int style; uint32_t seed; float swing; std::vector<std::pair<const char*, float>> values; };
const std::vector<PulsePreset>& pulsePresets();

class PulseProcessor : public juce::AudioProcessor
{
public:
    PulseProcessor();
    ~PulseProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return (int) pulsePresets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override { return pulsePresets()[(size_t) juce::jlimit (0, getNumPrograms() - 1, index)].name; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static bool isMidiOnlyBuild();

    // ---- pattern data (lock free, shared with the editor)
    std::atomic<uint8_t>& step (int pattern, int lane, int idx) { return steps[(size_t) pattern][(size_t) lane][(size_t) idx]; }
    uint8_t getStep (int pattern, int lane, int idx) const { return steps[(size_t) pattern][(size_t) lane][(size_t) idx].load (std::memory_order_relaxed); }
    void clearPattern (int pattern);
    void copyPattern (int from, int to);
    void generate (int style, uint32_t seed);   // fills the current pattern + lane params from a style
    void loadPreset (int index);
    void loadInit();
    juce::String getCurrentPresetName() const { return presetName; }
    int currentPatternIndex() const { return currentPattern.load(); }
    std::array<uint8_t, kMaxSteps> displaySteps (int lane) const;   // effective (grid or Euclid) for the current pattern

    // ---- UI feeds
    std::atomic<int> currentStep[kNumLanes] {};
    std::atomic<int> laneFlash[kNumLanes] {};
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::atomic<double> displayBeat { 0.0 };
    std::atomic<bool> transportRunning { false };
    std::function<void()> onPresetChanged;

    juce::AudioProcessorValueTreeState apvts;

private:
    PulseParamCache cache;
    Sequencer seq;
    DrumKit kit;
    lucid::EffectsChain bus;
    lucid::SynthParams busParams;
    std::array<VoiceParams, kNumLanes> voiceParams;
    std::array<std::array<std::array<std::atomic<uint8_t>, kMaxSteps>, kNumLanes>, kNumPatterns> steps;
    std::atomic<int> currentPattern { 0 };
    std::vector<SeqEvent> events;
    juce::AudioBuffer<float> renderBuffer;
    juce::MidiBuffer midiOut;
    int currentPreset = 0;
    juce::String presetName { "Init" };
    double sampleRate = 48000.0;

    void syncPatternToSequencer();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PulseProcessor)
};

} // namespace pulse
