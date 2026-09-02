#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PulseProcessor.h"
#include "../Source/ui/LookAndFeel.h"
#include "../Source/ui/Widgets.h"

namespace pulse {

// Clickable 8 x 32 step grid with playhead, lane length and step states.
// The static content (step colours, grid lines) is cached to an Image and only rebuilt when the
// pattern/lane data actually changes; each timer tick only repaints the small playhead cells that
// moved. This keeps the 30 Hz UI timer from forcing a full redraw of the whole grid every frame,
// which otherwise shows as flicker because `content` (the editor's scaled root) re-rasterises its
// entire cached image whenever anything inside it repaints.
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (PulseProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void refresh();
    void invalidateContent() { contentDirty = true; }
    std::function<void (int)> onLaneSelected;
    int selectedLane = 0;
    static constexpr int kRowH = 44;
private:
    PulseProcessor& proc;
    int paintState = 1, lastCell = -1;
    juce::Point<int> cellAt (juce::Point<float> pos) const;
    void setCell (int lane, int idx, int state);

    juce::Image background;
    bool contentDirty = true;
    uint64_t lastHash = 0;
    int lastPlayStep[kNumLanes];
    std::array<std::array<uint8_t, kMaxSteps>, kNumLanes> cachedSteps {};
    int cachedNumSteps[kNumLanes] {};

    uint64_t computeHash() const;
    void rebuildBackground();
};

// Per-lane strip left of the grid: on, name, steps, division/mode, euclid controls, plus a
// one-click "METER" preset that sets Steps+Division to a common musical time signature (e.g.
// 7/8, 5/4) so lanes can be given genuinely different, musically-named lengths without having to
// compute step counts by hand.
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
    juce::TextButton meterButton { "M" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAtt, hitsAtt, rotAtt;
    std::atomic<float>* euclidValue; std::atomic<float>* modeValue;
    int flash = 0;
    void showMeterMenu();
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

// Small handle that lets the user drag the current pattern out of the plug-in window as a
// standard MIDI file, dropping it straight into a track in the host DAW (Logic, Ableton, Cubase,
// Reaper, ...). The file is rendered deterministically from the same sequencer core that plays
// the pattern live, so what you drag out is exactly what you hear.
class MidiDragHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MidiDragHandle (PulseProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    int bars = 4;
private:
    PulseProcessor& proc;
    bool hover = false, dragging = false;
    juce::Point<float> dragStart;
};

class PulseEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
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
    // drag-to-DAW export
    MidiDragHandle dragHandle;
    juce::ComboBox dragBarsBox;
    void selectLane (int l);
    void timerCallback() override;
    void showPresetMenu();
    void showGenerateMenu();
    void updatePatternButtons();
};

} // namespace pulse
