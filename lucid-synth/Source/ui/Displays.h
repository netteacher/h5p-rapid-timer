// LUCID Synth - real-time displays: wavetable stack, filter response + analyser, envelopes, LFOs, scope
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"
#include "LookAndFeel.h"

namespace lucid {

class WavetableDisplay : public juce::Component
{
public:
    WavetableDisplay (LucidAudioProcessor& p, int oscIndex, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void refresh(); // called by the editor timer
private:
    LucidAudioProcessor& proc; int osc; juce::Colour colour;
    std::atomic<float>* table; std::atomic<float>* morph; std::atomic<float>* engine; std::atomic<float>* shape; std::atomic<float>* pw; std::atomic<float>* on;
    float lastMorph = -1.0f; int lastTable = -1, lastEngine = -1, lastShape = -1; float lastPw = -1.0f;
};

class FilterDisplay : public juce::Component
{
public:
    explicit FilterDisplay (LucidAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void refresh();
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    LucidAudioProcessor& proc;
    struct F { std::atomic<float> *on, *type, *cutoff, *res; juce::RangedAudioParameter *cutoffParam, *resParam; } f[kNumFilters];
    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { 2048, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, 4096> fftData {};
    std::array<float, 512> smoothed {};
    int dragging = -1;
    float xToHz (float x) const;
    float hzToX (float hz) const;
    float dbToY (float db) const;
    juce::Point<float> handlePos (int i) const;
};

class EnvelopeDisplay : public juce::Component
{
public:
    EnvelopeDisplay (LucidAudioProcessor& p, int envIndex, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void refresh();
private:
    LucidAudioProcessor& proc; int env; juce::Colour colour;
    std::atomic<float> *a, *d, *s, *r, *curve;
    float level = 0.0f;
};

class LfoDisplay : public juce::Component
{
public:
    LfoDisplay (LucidAudioProcessor& p, int lfoIndex, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void refresh();
private:
    LucidAudioProcessor& proc; int lfo; juce::Colour colour;
    std::atomic<float> *shape, *phase;
    float value = 0.0f; float phaseAnim = 0.0f;
};

// Oscilloscope + stereo peak meter for the header
class ScopeMeter : public juce::Component
{
public:
    explicit ScopeMeter (LucidAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void refresh();
private:
    LucidAudioProcessor& proc;
    std::array<float, 512> samples {};
    float peakL = 0.0f, peakR = 0.0f;
};

} // namespace lucid
