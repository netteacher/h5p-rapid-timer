// LUCID - shared UI widgets (knob, choice, switch, panel) attached to APVTS parameters
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

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


// lay a row of components out with equal width
void layoutRow (juce::Rectangle<int> row, std::initializer_list<juce::Component*> comps, int gap = 4);

} // namespace lucid
