#include "PluginEditor.h"

namespace lucid {
using namespace juce;

LucidAudioProcessorEditor::LucidAudioProcessorEditor (LucidAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      header (p), oscA (p, 0), oscB (p, 1), filters (p), envs (p), lfos (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    content.setSize (kBaseWidth, kBaseHeight);
    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<Component*> { &header, &oscA, &oscB, &filters, &envs, &lfos, &tabs, &keyboard }) content.addAndMakeVisible (c);

    tabs.addTab ("Mod Matrix", colours::panel, new ModMatrixPanel (p), true);
    tabs.addTab ("Effects", colours::panel, new FxPanel (p), true);
    tabs.addTab ("Voicing & Mix", colours::panel, new GlobalPanel (p), true);
    tabs.setTabBarDepth (26);
    tabs.setOutline (0);

    keyboard.setKeyWidth (22.0f);
    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setScrollButtonsVisible (true);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);
    setResizeLimits (kBaseWidth / 2, kBaseHeight / 2, kBaseWidth * 2, kBaseHeight * 2);
    setSize (kBaseWidth, kBaseHeight);
    startTimerHz (30);
}

LucidAudioProcessorEditor::~LucidAudioProcessorEditor()
{
    proc.presets.onPresetChanged = nullptr;
    setLookAndFeel (nullptr);
}

void LucidAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (colours::background);
}

void LucidAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseWidth;
    content.setTransform (AffineTransform::scale (scale));
    content.setBounds (0, 0, kBaseWidth, kBaseHeight);

    auto r = content.getLocalBounds();
    header.setBounds (r.removeFromTop (58));
    r = r.reduced (8, 0);
    r.removeFromTop (8);
    auto row1 = r.removeFromTop (264);
    oscA.setBounds (row1.removeFromLeft (362)); row1.removeFromLeft (8);
    oscB.setBounds (row1.removeFromLeft (362)); row1.removeFromLeft (8);
    filters.setBounds (row1);
    r.removeFromTop (8);
    auto row2 = r.removeFromTop (214);
    envs.setBounds (row2.removeFromLeft (610)); row2.removeFromLeft (8);
    lfos.setBounds (row2);
    r.removeFromTop (6);
    keyboard.setBounds (r.removeFromBottom (52));
    r.removeFromBottom (6);
    tabs.setBounds (r);
}

void LucidAudioProcessorEditor::timerCallback()
{
    header.refresh();
    oscA.refresh(); oscB.refresh();
    filters.refresh();
    envs.refresh(); lfos.refresh();
}

} // namespace lucid
