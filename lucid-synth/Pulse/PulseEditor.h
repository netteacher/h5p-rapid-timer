#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PulseProcessor.h"
#include "../Source/ui/LookAndFeel.h"
#include "../Source/ui/Widgets.h"

namespace pulse {

// Clickable 8 x 32 step grid with playhead, lane length and step states
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (PulseProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void refresh();
    std::function<void (int)> onLaneSelected;
    int selectedLane = 0;
    static constexpr int kRowH = 44;
private:
    PulseProcessor& proc;
    int paintState = 1, lastCell = -1;
    juce::Point<int> cellAt (juce::Point<float> pos) const;
    void setCell (int lane, int idx, int state);
};

// Per-lane strip left of the grid: on, name, steps, division/mode, euclid controls
class LaneStrip : public juce::Component
{
public:
    LaneStrip (PulseProcessor& p, int lane);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    bool selected = false;
private:
    PulseProcessor& proc; int lane;
    lucid::Switch on, euclid;
    juce::Slider steps, hits, rot;
    lucid::Choice div, mode, span;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAtt, hitsAtt, rotAtt;
    std::atomic<float>* euclidValue; std::atomic<float>* modeValue;
    int flash = 0;
};

// Controls of the selected lane (groove + voice)
class LaneInspector : public juce::Component
{
public:
    LaneInspector (PulseProcessor& p, int lane);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    int lane;
    lucid::Knob prob, swing, nudge, ratchet, gate, vel, accent, note, chan;
    std::unique_ptr<lucid::Knob> level, pan, tune, decay, tone, c1, c2, c3;
};

class PulseEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kBaseWidth = 1120, kBaseHeight = 640;
    explicit PulseEditor (PulseProcessor&);
    ~PulseEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
private:
    PulseProcessor& proc;
    lucid::LucidLookAndFeel lnf;
    juce::Component content;
    // header
    juce::TextButton presetButton, generateButton { "Generate" }, patternButtons[kNumPatterns], runButton { "Run" }, fillButton { "Fill" }, syncButton { "Host Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAtt, fillAtt, syncAtt;
    lucid::Knob swing, humTime, humVel, density, master, busDrive, busThresh;
    lucid::Switch busComp, busLimit;
    juce::Label transportLabel;
    // body
    StepGrid grid;
    std::unique_ptr<LaneStrip> strips[kNumLanes];
    std::unique_ptr<LaneInspector> inspectors[kNumLanes];
    int selectedLane = 0;
    void selectLane (int l);
    void timerCallback() override;
    void showPresetMenu();
    void showGenerateMenu();
    void updatePatternButtons();
};

} // namespace pulse
