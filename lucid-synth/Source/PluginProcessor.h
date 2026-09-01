// LUCID Synth - AudioProcessor: hosts the engine, parameters, presets and UI data feeds
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/SynthEngine.h"
#include "Parameters.h"
#include "Presets.h"

namespace lucid {

// Lock-free feed of output samples for the editor's analyser / oscilloscope
class ScopeFeed
{
public:
    static constexpr int kSize = 8192;
    void push (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            buffer[(size_t) writePos] = 0.5f * (l[i] + r[i]);
            writePos = (writePos + 1) & (kSize - 1);
        }
        written.store (writePos, std::memory_order_release);
    }
    // Copies the most recent 'n' samples into dest (n <= kSize)
    void readLatest (float* dest, int n) const
    {
        const int end = written.load (std::memory_order_acquire);
        int start = (end - n + kSize) & (kSize - 1);
        for (int i = 0; i < n; ++i) { dest[i] = buffer[(size_t) start]; start = (start + 1) & (kSize - 1); }
    }
private:
    std::array<float, kSize> buffer {};
    int writePos = 0;
    std::atomic<int> written { 0 };
};

class LucidAudioProcessor : public juce::AudioProcessor
{
public:
    LucidAudioProcessor();
    ~LucidAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LUCID"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return presets.getNumPresets(); }
    int getCurrentProgram() override { return presets.getCurrentIndex(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return presets.getName (index); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- access for the editor
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    juce::MidiKeyboardState keyboardState;
    ScopeFeed scopeFeed;
    const SynthEngine& getEngine() const { return engine; }
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::atomic<float> envDisplay[kNumEnvs] { { 0.0f }, { 0.0f }, { 0.0f } };
    std::atomic<float> lfoDisplay[kNumLfos] { { 0.0f }, { 0.0f }, { 0.0f } };
    std::atomic<int> activeVoices { 0 };
    std::atomic<float> limiterGr { 0.0f }, compGr { 0.0f };
    double getCurrentSampleRate() const { return currentSampleRate; }

private:
    SynthEngine engine;
    ParameterCache cache;
    SynthParams params;
    double currentSampleRate = 48000.0;
    juce::AudioBuffer<float> renderBuffer;

    void handleMidi (const juce::MidiMessage& m);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LucidAudioProcessor)
};

} // namespace lucid
