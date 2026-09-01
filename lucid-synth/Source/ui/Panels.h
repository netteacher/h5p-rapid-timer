// LUCID Synth - reusable widgets and the synth section panels
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "Displays.h"

namespace lucid {

using APVTS = juce::AudioProcessorValueTreeState;

// Rotary knob with a title label and value readout, attached to a parameter
class Knob : public juce::Component
{
public:
    Knob (APVTS& apvts, const juce::String& paramId, const juce::String& title, juce::Colour accent);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Combo box with optional title, attached to a choice parameter
class Choice : public juce::Component
{
public:
    Choice (APVTS& apvts, const juce::String& paramId, const juce::String& title = {});
    void resized() override;
    juce::ComboBox box;
    juce::Label label;
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// Toggle switch attached to a bool parameter
// Toggle switch attached to a bool parameter. With 'titleAbove' the text becomes a small label
// above a centred switch (same footprint as a knob), otherwise the text sits right of the switch.
class Switch : public juce::Component
{
public:
    Switch (APVTS& apvts, const juce::String& paramId, const juce::String& text, juce::Colour accent, bool titleAbove = false);
    void resized() override;
    juce::ToggleButton button;
    juce::Label label;
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Rounded panel with a title strip
class Panel : public juce::Component
{
public:
    Panel (const juce::String& title, juce::Colour accent);
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (8).withTrimmedTop (22); }
    juce::Rectangle<int> titleArea() const { return getLocalBounds().reduced (8).removeFromTop (22); }
protected:
    juce::String title; juce::Colour accent;
};

// ---------------------------------------------------------------------------- sections
class OscPanel : public Panel
{
public:
    OscPanel (LucidAudioProcessor& p, int oscIndex);
    void resized() override;
    void refresh();
private:
    int osc; juce::Colour c;
    Switch on; Choice engine, table, shape;
    WavetableDisplay display;
    Knob morph, pw, level, pan, oct, semi, fine, drift, unison, detune, spread, blend, phase;
    Switch retrig;
    std::atomic<float>* engineValue;
    int lastEngine = -1;
};

class FilterPanel : public Panel
{
public:
    explicit FilterPanel (LucidAudioProcessor& p);
    void resized() override;
    void refresh() { display.refresh(); }
private:
    FilterDisplay display;
    Choice routing; Knob mix;
    struct Unit { Unit (LucidAudioProcessor& p, int i); Switch on; Choice type; Knob cutoff, res, drive, env, key; juce::Label title; };
    std::unique_ptr<Unit> units[kNumFilters];
};

class EnvPanel : public Panel
{
public:
    explicit EnvPanel (LucidAudioProcessor& p);
    void resized() override;
    void refresh() { for (auto& u : units) u->display.refresh(); }
private:
    struct Unit { Unit (LucidAudioProcessor& p, int i); juce::Label title; EnvelopeDisplay display; Knob a, d, s, r, curve, vel; };
    std::unique_ptr<Unit> units[kNumEnvs];
};

class LfoPanel : public Panel
{
public:
    explicit LfoPanel (LucidAudioProcessor& p);
    void resized() override;
    void refresh() { for (auto& u : units) u->display.refresh(); }
private:
    struct Unit { Unit (LucidAudioProcessor& p, int i); juce::Label title; Choice shape; LfoDisplay display; Knob rate, phase, fade, smooth; Switch sync, mono; Choice div; };
    std::unique_ptr<Unit> units[kNumLfos];
};

class ModMatrixPanel : public juce::Component
{
public:
    explicit ModMatrixPanel (LucidAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    struct Row { Row (LucidAudioProcessor& p, int i); juce::Label index; Choice src, dst; juce::Slider amount; std::unique_ptr<APVTS::SliderAttachment> att; };
    std::unique_ptr<Row> rows[kNumModSlots];
};

class FxPanel : public juce::Component
{
public:
    explicit FxPanel (LucidAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    struct Section
    {
        Section (const juce::String& n, int w) : name (n), width (w) {}
        juce::String name; int width;
        std::unique_ptr<Switch> on;
        std::vector<std::unique_ptr<juce::Component>> items; // knobs / choices laid out in a grid
    };
    std::vector<std::unique_ptr<Section>> sections;
    LucidAudioProcessor& proc;
};

class GlobalPanel : public juce::Component
{
public:
    explicit GlobalPanel (LucidAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Knob subLevel, noiseLevel, noiseColor, fm, ring, glide, velAmp, pan;
    Choice subOct, subShape, voiceMode;
    Knob voices, bendRange;
    juce::Label subTitle, noiseTitle, mixTitle, voiceTitle;
};

// Header with logo, preset browser, macros, scope and master
class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (LucidAudioProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
private:
    LucidAudioProcessor& proc;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, presetButton, saveButton { "Save" };
    juce::Label categoryLabel, voicesLabel;
    std::unique_ptr<Knob> macros[kNumMacros];
    Knob master;
    ScopeMeter scope;
    void showPresetMenu();
    void savePreset();
    void updatePresetName();
};

} // namespace lucid
