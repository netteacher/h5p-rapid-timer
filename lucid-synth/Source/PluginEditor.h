#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Panels.h"

namespace lucid {

class LucidAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kBaseWidth = 1180, kBaseHeight = 780;

    explicit LucidAudioProcessorEditor (LucidAudioProcessor&);
    ~LucidAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LucidAudioProcessor& proc;
    LucidLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // All content lives in a fixed-size component that is scaled with the window
    juce::Component content;
    HeaderBar header;
    OscPanel oscA, oscB;
    FilterPanel filters;
    EnvPanel envs;
    LfoPanel lfos;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::MidiKeyboardComponent keyboard;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LucidAudioProcessorEditor)
};

} // namespace lucid
