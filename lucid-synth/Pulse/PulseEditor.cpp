#include "PulseEditor.h"

namespace pulse {
using namespace juce;
using namespace lucid;

static const Colour laneColours[kNumLanes] = { colours::accent, colours::accentB, colours::fx, colours::lfo, colours::lfo.withAlpha (0.8f), colours::filter, colours::envelope, colours::mod };
static const Colour stateColours[5] = { Colour (0x00000000), Colour (0xffe9ecf1), Colour (0xffffb347), Colour (0xff6b7488), Colour (0xff45d0ff) };

// ============================================================================ StepGrid
StepGrid::StepGrid (PulseProcessor& p) : proc (p)
{
    for (auto& s : lastPlayStep) s = -1;
    setOpaque (true);
}

uint64_t StepGrid::computeHash() const
{
    uint64_t h = 1469598103934665603ull; // FNV-1a offset basis; sequential mixing already encodes position
    auto mix = [&h] (uint64_t v) { h ^= v; h *= 1099511628211ull; };
    mix ((uint64_t) selectedLane + 1);
    for (int l = 0; l < kNumLanes; ++l)
    {
        const auto stepsArr = proc.displaySteps (l);
        const int numSteps = roundToInt (proc.apvts.getRawParameterValue (ids::lane (l, "steps"))->load());
        const bool euclid = proc.apvts.getRawParameterValue (ids::lane (l, "euclid"))->load() >= 0.5f;
        const bool on = proc.apvts.getRawParameterValue (ids::lane (l, "on"))->load() >= 0.5f;
        mix ((uint64_t) numSteps * 4u + (euclid ? 2u : 0u) + (on ? 1u : 0u) + 7u);
        for (int i = 0; i < kMaxSteps; ++i) mix ((uint64_t) stepsArr[(size_t) i] + 3u);
    }
    return h;
}

void StepGrid::rebuildBackground()
{
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    background = Image (Image::ARGB, w, h, true);
    Graphics g (background);
    const float cw = (float) w / (float) kMaxSteps;
    for (int l = 0; l < kNumLanes; ++l)
    {
        const auto stepsArr = proc.displaySteps (l);
        const int numSteps = roundToInt (proc.apvts.getRawParameterValue (ids::lane (l, "steps"))->load());
        const bool euclid = proc.apvts.getRawParameterValue (ids::lane (l, "euclid"))->load() >= 0.5f;
        const bool on = proc.apvts.getRawParameterValue (ids::lane (l, "on"))->load() >= 0.5f;
        const float y = (float) l * kRowH;
        cachedNumSteps[l] = numSteps;
        if (l == selectedLane) { g.setColour (colours::panelLight.withAlpha (0.5f)); g.fillRect (0.0f, y, (float) w, (float) kRowH); }
        for (int i = 0; i < kMaxSteps; ++i)
        {
            const auto cell = Rectangle<float> ((float) i * cw, y, cw, (float) kRowH).reduced (2.0f, 7.0f);
            const bool inRange = i < numSteps;
            const int st = inRange ? stepsArr[(size_t) i] : 0;
            cachedSteps[(size_t) l][(size_t) i] = (uint8_t) st;
            g.setColour ((i / 4) % 2 == 0 ? colours::background : colours::background.brighter (0.06f));
            g.fillRoundedRectangle (cell, 3.0f);
            if (! inRange) continue;
            if (st != 0)
            {
                auto c = laneColours[l];
                if (st == 3) c = c.withAlpha (0.45f);
                if (st == 4) c = c.withAlpha (0.7f);
                g.setColour (on ? c : c.withAlpha (0.3f));
                if (st == 2) g.fillRoundedRectangle (cell, 3.0f);
                else if (st == 4) { g.drawRoundedRectangle (cell.reduced (1.0f), 3.0f, 1.5f); g.fillRoundedRectangle (cell.reduced (cell.getWidth() * 0.3f, cell.getHeight() * 0.3f), 2.0f); }
                else g.fillRoundedRectangle (cell.reduced (0.0f, st == 3 ? 5.0f : 2.0f), 3.0f);
            }
            else { g.setColour (colours::outline.withAlpha (0.6f)); g.drawRoundedRectangle (cell, 3.0f, 1.0f); }
        }
        if (euclid) { g.setColour (colours::mod); g.setFont (LucidLookAndFeel::font (9.0f, true)); g.drawText ("EUCLID", Rectangle<int> (w - 50, (int) y + 2, 46, 12), Justification::centredRight); }
        if (numSteps < kMaxSteps) { g.setColour (laneColours[l].withAlpha (0.5f)); g.fillRect ((float) numSteps * cw - 1.0f, y + 6.0f, 2.0f, (float) kRowH - 12.0f); }
    }
    contentDirty = false;
    for (auto& s : lastPlayStep) s = -1; // force the playhead overlay to redraw against the fresh image
}

void StepGrid::resized() { contentDirty = true; background = Image(); }

void StepGrid::refresh()
{
    const uint64_t h = computeHash();
    if (h != lastHash) { lastHash = h; contentDirty = true; repaint(); return; }
    // nothing structural changed: only redraw the cells the playhead entered/left, not the whole grid
    const float cw = (float) getWidth() / (float) kMaxSteps;
    for (int l = 0; l < kNumLanes; ++l)
    {
        const int cur = proc.currentStep[l].load();
        if (cur == lastPlayStep[l]) continue;
        if (lastPlayStep[l] >= 0) repaint (Rectangle<float> ((float) lastPlayStep[l] * cw, (float) l * kRowH, cw, (float) kRowH).getSmallestIntegerContainer());
        if (cur >= 0) repaint (Rectangle<float> ((float) cur * cw, (float) l * kRowH, cw, (float) kRowH).getSmallestIntegerContainer());
        lastPlayStep[l] = cur;
    }
}

Point<int> StepGrid::cellAt (Point<float> pos) const
{
    const float cw = (float) getWidth() / (float) kMaxSteps;
    return { jlimit (0, kMaxSteps - 1, (int) (pos.x / cw)), jlimit (0, kNumLanes - 1, (int) (pos.y / (float) kRowH)) };
}
void StepGrid::setCell (int lane, int idx, int state)
{
    proc.step (proc.currentPatternIndex(), lane, idx).store ((uint8_t) state);
    contentDirty = true;
    repaint();
}
void StepGrid::mouseDown (const MouseEvent& e)
{
    const auto c = cellAt (e.position);
    if (c.y != selectedLane) { selectedLane = c.y; contentDirty = true; if (onLaneSelected) onLaneSelected (c.y); }
    if (auto* p = proc.apvts.getRawParameterValue (ids::lane (c.y, "euclid")); p != nullptr && p->load() >= 0.5f) return; // euclid lanes are generated
    const int cur = proc.getStep (proc.currentPatternIndex(), c.y, c.x);
    if (e.mods.isRightButtonDown()) paintState = 0;
    else if (e.mods.isShiftDown()) paintState = cur == 2 ? 1 : 2;      // shift: accent
    else if (e.mods.isAltDown()) paintState = cur == 3 ? 1 : 3;        // alt: ghost
    else if (e.mods.isCommandDown() || e.mods.isCtrlDown()) paintState = cur == 4 ? 1 : 4; // cmd: maybe
    else paintState = cur == 0 ? 1 : 0;
    lastCell = c.y * 100 + c.x;
    setCell (c.y, c.x, paintState);
}
void StepGrid::mouseDrag (const MouseEvent& e)
{
    const auto c = cellAt (e.position);
    if (c.y != selectedLane) return;
    const int id = c.y * 100 + c.x;
    if (id == lastCell) return;
    lastCell = id;
    if (auto* p = proc.apvts.getRawParameterValue (ids::lane (c.y, "euclid")); p != nullptr && p->load() >= 0.5f) return;
    setCell (c.y, c.x, paintState);
}
void StepGrid::paint (Graphics& g)
{
    if (contentDirty || ! background.isValid() || background.getWidth() != getWidth() || background.getHeight() != getHeight())
        rebuildBackground();
    g.drawImageAt (background, 0, 0);
    const float cw = (float) getWidth() / (float) kMaxSteps;
    for (int l = 0; l < kNumLanes; ++l)
    {
        const int cur = proc.currentStep[l].load();
        if (cur < 0 || cur >= cachedNumSteps[l]) continue;
        const auto cell = Rectangle<float> ((float) cur * cw, (float) l * kRowH, cw, (float) kRowH).reduced (2.0f, 7.0f);
        const bool hasNote = cachedSteps[(size_t) l][(size_t) cur] != 0;
        g.setColour (Colours::white.withAlpha (hasNote ? 0.35f : 0.12f));
        g.fillRoundedRectangle (cell.expanded (1.0f, 3.0f), 3.0f);
    }
}

// ============================================================================ LaneStrip
static void styleNumber (Slider& s, Colour c)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 40, 14);
    s.setColour (Slider::rotarySliderFillColourId, c);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
}
LaneStrip::LaneStrip (PulseProcessor& p, int l)
    : proc (p), lane (l),
      on (p.apvts, ids::lane (l, "on"), "", laneColours[l]), euclid (p.apvts, ids::lane (l, "euclid"), "EUC", laneColours[l], true),
      div (p.apvts, ids::lane (l, "div")), mode (p.apvts, ids::lane (l, "mode")), span (p.apvts, ids::lane (l, "span"))
{
    styleNumber (steps, laneColours[l]); styleNumber (hits, laneColours[l]); styleNumber (rot, laneColours[l]);
    stepsAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids::lane (l, "steps"), steps);
    hitsAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids::lane (l, "hits"), hits);
    rotAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids::lane (l, "rot"), rot);
    for (auto* c : std::initializer_list<Component*> { &on, &euclid, &steps, &hits, &rot, &div, &mode, &span, &meterButton }) addAndMakeVisible (c);
    euclidValue = p.apvts.getRawParameterValue (ids::lane (l, "euclid"));
    modeValue = p.apvts.getRawParameterValue (ids::lane (l, "mode"));
    steps.setTooltip ("Steps"); hits.setTooltip ("Euclid hits"); rot.setTooltip ("Euclid rotation");
    div.box.setTooltip ("Clock division (polymeter)"); mode.box.setTooltip ("Polymeter: own length & division. Polyrhythm: steps spread over the span."); span.box.setTooltip ("Polyrhythm span");
    meterButton.setTooltip ("Quick meter: sets Steps + Division to a common time signature (e.g. 7/8, 5/4) in one click, so this lane can run in a different meter than the others");
    meterButton.setColour (TextButton::buttonColourId, laneColours[l].withAlpha (0.16f));
    meterButton.onClick = [this] { showMeterMenu(); };
}
void LaneStrip::showMeterMenu()
{
    struct MeterPreset { const char* name; int steps; };
    static const MeterPreset presets[] = {
        { "2/4",  8 }, { "3/4", 12 }, { "4/4", 16 }, { "5/4", 20 }, { "7/4", 28 },
        { "3/8",  6 }, { "5/8", 10 }, { "6/8", 12 }, { "7/8", 14 }, { "9/8", 18 }, { "11/8", 22 }, { "12/8", 24 },
    };
    PopupMenu m;
    m.addSectionHeader ("Set lane meter (16th-note grid)");
    for (int i = 0; i < (int) (sizeof (presets) / sizeof (presets[0])); ++i) m.addItem (i + 1, presets[i].name);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (meterButton), [this] (int r)
    {
        if (r <= 0) return;
        const auto& pr = presets[r - 1];
        auto setP = [this] (const String& id, float v) { if (auto* par = proc.apvts.getParameter (id)) par->setValueNotifyingHost (par->convertTo0to1 (v)); };
        setP (ids::lane (lane, "steps"), (float) pr.steps);
        setP (ids::lane (lane, "div"), 3.0f);   // 1/16
        setP (ids::lane (lane, "mode"), 0.0f);  // Polymeter: this lane keeps its own length independently of the others
    });
}
void LaneStrip::refresh()
{
    const bool e = euclidValue->load() >= 0.5f;
    hits.setEnabled (e); rot.setEnabled (e);
    const bool poly = modeValue->load() >= 0.5f;
    div.setVisible (! poly); span.setVisible (poly);
    const int f = proc.laneFlash[lane].load();
    if (f > 0) proc.laneFlash[lane].store (f - 1);
    if (f != flash) { flash = f; repaint(); }
}
void LaneStrip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (selected) { g.setColour (colours::panelLight.withAlpha (0.5f)); g.fillRect (r); }
    g.setColour (laneColours[lane].withAlpha (flash > 0 ? 1.0f : 0.5f));
    g.fillRoundedRectangle (2.0f, r.getCentreY() - 8.0f, 3.0f, 16.0f, 1.5f);
    g.setColour (colours::text); g.setFont (LucidLookAndFeel::font (11.0f, true));
    g.drawText (voiceName (lane), Rectangle<int> (42, 0, 56, getHeight()), Justification::centredLeft);
}
void LaneStrip::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (8);
    on.setBounds (r.removeFromLeft (30).withSizeKeepingCentre (30, 18));
    r.removeFromLeft (52);
    steps.setBounds (r.removeFromLeft (38));
    meterButton.setBounds (r.removeFromLeft (30).withSizeKeepingCentre (26, 20));
    r.removeFromLeft (2);
    auto combos = r.removeFromLeft (70);
    mode.setBounds (combos.removeFromTop (r.getHeight() / 2).reduced (2, 1));
    div.setBounds (combos.reduced (2, 1)); span.setBounds (combos.reduced (2, 1));
    euclid.setBounds (r.removeFromLeft (32).withSizeKeepingCentre (32, 32));
    hits.setBounds (r.removeFromLeft (38));
    rot.setBounds (r.removeFromLeft (38));
}

// ============================================================================ LaneInspector
LaneInspector::LaneInspector (PulseProcessor& p, int l)
    : lane (l),
      prob (p.apvts, ids::lane (l, "prob"), "PROB", laneColours[l]), swing (p.apvts, ids::lane (l, "swing"), "SWING", laneColours[l]),
      nudge (p.apvts, ids::lane (l, "nudge"), "NUDGE", laneColours[l]), ratchet (p.apvts, ids::lane (l, "ratchet"), "RATCHET", laneColours[l]),
      gate (p.apvts, ids::lane (l, "gate"), "GATE", laneColours[l]), vel (p.apvts, ids::lane (l, "vel"), "VELOCITY", laneColours[l]),
      accent (p.apvts, ids::lane (l, "accent"), "ACCENT", laneColours[l]), note (p.apvts, ids::lane (l, "note"), "MIDI NOTE", laneColours[l]),
      chan (p.apvts, ids::lane (l, "chan"), "MIDI CH", laneColours[l])
{
    for (auto* c : std::initializer_list<Component*> { &prob, &swing, &nudge, &ratchet, &gate, &vel, &accent, &note, &chan }) addAndMakeVisible (c);
    if (! PulseProcessor::isMidiOnlyBuild())
    {
        const auto c = laneColours[l];
        level = std::make_unique<Knob> (p.apvts, ids::lane (l, "level"), "LEVEL", c); pan = std::make_unique<Knob> (p.apvts, ids::lane (l, "pan"), "PAN", c);
        tune = std::make_unique<Knob> (p.apvts, ids::lane (l, "tune"), "TUNE", c); decay = std::make_unique<Knob> (p.apvts, ids::lane (l, "decay"), "DECAY", c);
        tone = std::make_unique<Knob> (p.apvts, ids::lane (l, "tone"), "TONE", c);
        c1 = std::make_unique<Knob> (p.apvts, ids::lane (l, "c1"), voiceCtlName (l, 0), c); c2 = std::make_unique<Knob> (p.apvts, ids::lane (l, "c2"), voiceCtlName (l, 1), c);
        c3 = std::make_unique<Knob> (p.apvts, ids::lane (l, "c3"), voiceCtlName (l, 2), c);
        for (auto* k : { level.get(), pan.get(), tune.get(), decay.get(), tone.get(), c1.get(), c2.get(), c3.get() }) addAndMakeVisible (k);
    }
}
void LaneInspector::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel); g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::outline); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (laneColours[lane]); g.fillRoundedRectangle (8.0f, 8.0f, 3.0f, 14.0f, 1.5f);
    g.setFont (LucidLookAndFeel::font (12.0f, true)); g.setColour (colours::text);
    g.drawText (String (voiceName (lane)) + "  -  GROOVE", 18, 6, 200, 18, Justification::centredLeft);
    if (level != nullptr) { g.drawText ("SOUND", getWidth() / 2 + 18, 6, 200, 18, Justification::centredLeft); g.setColour (laneColours[lane]); g.fillRoundedRectangle ((float) getWidth() / 2 + 8.0f, 8.0f, 3.0f, 14.0f, 1.5f); }
}
void LaneInspector::resized()
{
    auto r = getLocalBounds().reduced (8).withTrimmedTop (22);
    if (level != nullptr)
    {
        auto left = r.removeFromLeft (r.getWidth() / 2 - 4); r.removeFromLeft (8);
        layoutRow (left, { &prob, &swing, &nudge, &ratchet, &gate, &vel, &accent, &note }, 2);
        layoutRow (r, { level.get(), pan.get(), tune.get(), decay.get(), tone.get(), c1.get(), c2.get(), c3.get() }, 2);
        chan.setVisible (false);
    }
    else layoutRow (r, { &prob, &swing, &nudge, &ratchet, &gate, &vel, &accent, &note, &chan }, 4);
}

// ============================================================================ MidiDragHandle
MidiDragHandle::MidiDragHandle (PulseProcessor& p) : proc (p)
{
    setTooltip ("Drag into a track in your DAW to create a MIDI region from the current pattern (what you hear is what you get)");
    setMouseCursor (MouseCursor::DraggingHandCursor);
}
void MidiDragHandle::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (hover ? colours::mod.withAlpha (0.22f) : colours::panelLight.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (hover ? colours::mod : colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (colours::textDim);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillEllipse (r.getX() + 10.0f + (float) j * 6.0f, r.getCentreY() - 7.0f + (float) i * 7.0f, 3.0f, 3.0f);
    g.setColour (colours::text);
    g.setFont (LucidLookAndFeel::font (11.0f, true));
    g.drawText ("DRAG PATTERN TO DAW (MIDI)", r.withTrimmedLeft (30.0f), Justification::centredLeft);
}
void MidiDragHandle::mouseDown (const MouseEvent& e) { dragStart = e.position; dragging = false; }
void MidiDragHandle::mouseDrag (const MouseEvent& e)
{
    if (dragging) return;
    if (e.position.getDistanceFrom (dragStart) < 6.0f) return;
    dragging = true;
    auto file = proc.renderPatternToMidiFile (proc.currentPatternIndex(), bars, proc.lastKnownBpm.load());
    if (auto* dnd = DragAndDropContainer::findParentDragContainerFor (this))
        dnd->performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}
void MidiDragHandle::mouseUp (const MouseEvent&) { dragging = false; }

// ============================================================================ Editor
PulseEditor::PulseEditor (PulseProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      swing (p.apvts, "swing", "SWING", colours::mod), humTime (p.apvts, "humTime", "HUMAN T", colours::mod), humVel (p.apvts, "humVel", "HUMAN V", colours::mod),
      density (p.apvts, "density", "DENSITY", colours::mod), master (p.apvts, "master", "MASTER", colours::accent), busDrive (p.apvts, "busDrive", "DRIVE", colours::fx),
      busThresh (p.apvts, "busThresh", "COMP", colours::fx), busComp (p.apvts, "busComp", "COMP", colours::fx, true), busLimit (p.apvts, "busLimit", "LIMIT", colours::fx, true),
      grid (p), dragHandle (p)
{
    setLookAndFeel (&lnf);
    content.setSize (kBaseWidth, kBaseHeight);
    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<Component*> { &presetButton, &generateButton, &runButton, &fillButton, &syncButton, &swing, &humTime, &humVel, &density, &transportLabel, &grid, &dragHandle, &dragBarsBox })
        content.addAndMakeVisible (c);
    dragBarsBox.addItemList ({ "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars" }, 1);
    dragBarsBox.setSelectedItemIndex (2, dontSendNotification); // 4 bars
    dragBarsBox.setTooltip ("How many bars to render into the dragged MIDI file");
    dragBarsBox.onChange = [this]
    {
        static const int bars[] = { 1, 2, 4, 8, 16 };
        const int idx = jlimit (0, 4, dragBarsBox.getSelectedItemIndex());
        dragHandle.bars = bars[idx];
    };
    if (! PulseProcessor::isMidiOnlyBuild()) for (auto* c : std::initializer_list<Component*> { &master, &busDrive, &busThresh, &busComp, &busLimit }) content.addAndMakeVisible (c);
    for (int i = 0; i < kNumPatterns; ++i)
    {
        patternButtons[i].setButtonText (String::charToString ((juce_wchar) ('A' + i)));
        patternButtons[i].setClickingTogglesState (false);
        patternButtons[i].onClick = [this, i]
        {
            if (auto* par = proc.apvts.getParameter ("pattern")) par->setValueNotifyingHost (par->convertTo0to1 ((float) i));
            updatePatternButtons();
        };
        content.addAndMakeVisible (patternButtons[i]);
    }
    runButton.setClickingTogglesState (true); fillButton.setClickingTogglesState (true); syncButton.setClickingTogglesState (true);
    runAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "run", runButton);
    fillAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "fill", fillButton);
    syncAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "hostSync", syncButton);
    fillButton.setColour (TextButton::buttonOnColourId, colours::fx);
    runButton.setColour (TextButton::buttonOnColourId, colours::envelope);
    presetButton.onClick = [this] { showPresetMenu(); };
    generateButton.onClick = [this] { showGenerateMenu(); };
    generateButton.setColour (TextButton::buttonColourId, colours::mod.withAlpha (0.25f));
    transportLabel.setFont (LucidLookAndFeel::font (10.5f)); transportLabel.setColour (Label::textColourId, colours::textDim); transportLabel.setJustificationType (Justification::centredRight);

    for (int l = 0; l < kNumLanes; ++l)
    {
        strips[l] = std::make_unique<LaneStrip> (p, l); content.addAndMakeVisible (*strips[l]);
        inspectors[l] = std::make_unique<LaneInspector> (p, l); content.addChildComponent (*inspectors[l]);
    }
    grid.onLaneSelected = [this] (int l) { selectLane (l); };
    selectLane (0);
    proc.onPresetChanged = [this] { presetButton.setButtonText (proc.getCurrentPresetName()); updatePatternButtons(); grid.invalidateContent(); grid.repaint(); };
    presetButton.setButtonText (proc.getCurrentPresetName());
    updatePatternButtons();

    // Fixed size, not resizable: a scaling Component::setTransform on the root was used earlier,
    // but a transformed subtree forces JUCE to re-rasterise its whole cached image on every repaint
    // inside it, and some hosts (Logic's MIDI FX chain in particular, which sizes plugin windows
    // more aggressively than an instrument slot) can get into a resize back-and-forth with a
    // plugin that reports itself as resizable. Both show up as exactly the flicker/redraw
    // corruption reported for this plugin, so the window is now fixed at its native size.
    setResizable (false, false);
    setSize (kBaseWidth, kBaseHeight);
    startTimerHz (30);
}
PulseEditor::~PulseEditor() { proc.onPresetChanged = nullptr; setLookAndFeel (nullptr); }

void PulseEditor::selectLane (int l)
{
    selectedLane = l; grid.selectedLane = l;
    for (int i = 0; i < kNumLanes; ++i) { inspectors[i]->setVisible (i == l); strips[i]->selected = i == l; strips[i]->repaint(); }
    grid.invalidateContent();
    grid.repaint();
}
void PulseEditor::updatePatternButtons()
{
    const int cur = proc.currentPatternIndex();
    for (int i = 0; i < kNumPatterns; ++i) patternButtons[i].setToggleState (i == cur, dontSendNotification);
}
void PulseEditor::showPresetMenu()
{
    PopupMenu m;
    m.addItem (1, "Init (clear all)");
    m.addSeparator();
    const auto& all = pulsePresets();
    for (int i = 0; i < (int) all.size(); ++i) m.addItem (100 + i, all[(size_t) i].name, true, proc.getCurrentProgram() == i && proc.getCurrentPresetName() == all[(size_t) i].name);
    m.addSeparator();
    PopupMenu pat;
    pat.addItem (200, "Clear current pattern");
    for (int i = 0; i < kNumPatterns; ++i) if (i != proc.currentPatternIndex()) pat.addItem (300 + i, "Copy current to " + String::charToString ((juce_wchar) ('A' + i)));
    m.addSubMenu ("Pattern", pat);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (presetButton), [this] (int r)
    {
        if (r == 1) proc.loadInit();
        else if (r >= 100 && r < 200) proc.loadPreset (r - 100);
        else if (r == 200) proc.clearPattern (proc.currentPatternIndex());
        else if (r >= 300) proc.copyPattern (proc.currentPatternIndex(), r - 300);
        grid.invalidateContent();
        grid.repaint();
    });
}
void PulseEditor::showGenerateMenu()
{
    PopupMenu m;
    for (int i = 0; i < kNumStyles; ++i) m.addItem (1 + i, styleName (i));
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (generateButton), [this] (int r)
    {
        if (r > 0)
        {
            proc.generate (r - 1, (uint32_t) juce::Random::getSystemRandom().nextInt());
            grid.invalidateContent();
            grid.repaint();
        }
    });
}
void PulseEditor::paint (Graphics& g)
{
    g.fillAll (colours::background);
}
void PulseEditor::paintOverChildren (Graphics& g)
{
    // logo
    g.setFont (LucidLookAndFeel::font (22.0f, true));
    g.setGradientFill (ColourGradient (colours::accent, 20, 0, colours::mod, 160, 0, false));
    g.drawText ("LUCID PULSE", 20, 12, 160, 26, Justification::centredLeft);
    g.setFont (LucidLookAndFeel::font (9.0f)); g.setColour (colours::textDim);
    g.drawText (PulseProcessor::isMidiOnlyBuild() ? "POLYRHYTHMIC MIDI SEQUENCER" : "POLYRHYTHMIC DRUM MACHINE", 20, 38, 200, 12, Justification::centredLeft);
    // legend
    const int y = kBaseHeight - 34;
    g.setFont (LucidLookAndFeel::font (10.0f));
    int x = 20;
    auto swatch = [&] (Colour c, const String& text, bool outline = false)
    {
        auto r = Rectangle<float> ((float) x, (float) y + 2, 14.0f, 14.0f);
        g.setColour (c); if (outline) g.drawRoundedRectangle (r, 3.0f, 1.5f); else g.fillRoundedRectangle (r, 3.0f);
        g.setColour (colours::textDim); g.drawText (text, x + 19, y, 200, 18, Justification::centredLeft);
        x += 19 + (int) LucidLookAndFeel::font (10.0f).getStringWidthFloat (text) + 22;
    };
    swatch (colours::text, "Click = Step");
    swatch (colours::accentB, "Shift+Click = Accent");
    swatch (colours::text.withAlpha (0.45f), "Alt+Click = Ghost");
    swatch (colours::accent, "Cmd/Ctrl+Click = Maybe (probability)", true);
    swatch (colours::outline, "Right-Click = Clear", true);
    g.setColour (colours::textDim);
    g.drawText ("M: quick meter preset (7/8, 5/4, ...)   |   Polymeter: own length & clock per lane   |   Polyrhythm: N steps over 1/2/4 bars   |   EUC: Euclidean hits/rotation", 20, y + 18, kBaseWidth - 40, 14, Justification::centredLeft);
}
void PulseEditor::resized()
{
    content.setBounds (0, 0, kBaseWidth, kBaseHeight); // fixed-size window, always 1:1 - see the constructor
    auto r = content.getLocalBounds();
    auto header = r.removeFromTop (64).reduced (10, 8);
    header.removeFromLeft (170); // logo
    auto row = header.withHeight (24).withY (header.getY() + 2);
    presetButton.setBounds (row.removeFromLeft (170)); row.removeFromLeft (6);
    generateButton.setBounds (row.removeFromLeft (84)); row.removeFromLeft (14);
    for (auto& b : patternButtons) { b.setBounds (row.removeFromLeft (30)); row.removeFromLeft (3); }
    row.removeFromLeft (11);
    runButton.setBounds (row.removeFromLeft (52)); row.removeFromLeft (4);
    fillButton.setBounds (row.removeFromLeft (46)); row.removeFromLeft (4);
    syncButton.setBounds (row.removeFromLeft (80));
    auto knobs = header.withTrimmedLeft (row.getX() - header.getX() + 12).withHeight (58).withY (header.getY() - 4);
    if (! PulseProcessor::isMidiOnlyBuild())
    {
        master.setBounds (knobs.removeFromRight (60)); busLimit.setBounds (knobs.removeFromRight (44)); busComp.setBounds (knobs.removeFromRight (44));
        busThresh.setBounds (knobs.removeFromRight (56)); busDrive.setBounds (knobs.removeFromRight (56)); knobs.removeFromRight (16);
    }
    transportLabel.setBounds (knobs.removeFromRight (120).withHeight (20).withY (knobs.getY() + 20));
    layoutRow (knobs.removeFromLeft (4 * 58), { &swing, &humTime, &humVel, &density }, 2);

    r = r.reduced (10, 0);
    auto body = r.removeFromTop (kNumLanes * StepGrid::kRowH);
    auto stripCol = body.removeFromLeft (344);
    for (int l = 0; l < kNumLanes; ++l) strips[l]->setBounds (stripCol.removeFromTop (StepGrid::kRowH));
    grid.setBounds (body);
    r.removeFromTop (8);
    // all 8 inspectors share the SAME area (only the selected lane's is visible) - assign it once,
    // not per-lane, otherwise repeated removeFromTop() calls exhaust r and lanes 3-8 end up with a
    // zero-height (invisible) inspector the moment they're selected.
    auto inspectorArea = r.removeFromTop (110);
    for (auto& i : inspectors) i->setBounds (inspectorArea);
    r.removeFromTop (6);
    auto dragRow = r.removeFromTop (28);
    dragHandle.setBounds (dragRow.removeFromLeft (230));
    dragRow.removeFromLeft (8);
    dragBarsBox.setBounds (dragRow.removeFromLeft (90));
}
void PulseEditor::timerCallback()
{
    grid.refresh();
    for (auto& s : strips) s->refresh();
    const bool running = proc.transportRunning.load();
    const double beat = proc.displayBeat.load();
    const int bar = (int) std::floor (beat / 4.0) + 1, b = (int) std::floor (std::fmod (beat, 4.0)) + 1;
    transportLabel.setText (running ? String::formatted ("%d . %d", bar, b) : String ("stopped"), dontSendNotification);
    if (proc.currentPatternIndex() != [this] { for (int i = 0; i < kNumPatterns; ++i) if (patternButtons[i].getToggleState()) return i; return -1; }()) updatePatternButtons();
}

} // namespace pulse
