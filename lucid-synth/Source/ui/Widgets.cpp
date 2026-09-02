#include "Widgets.h"

namespace lucid {
using namespace juce;

// ============================================================================ widgets
Knob::Knob (APVTS& apvts, const String& paramId, const String& title, Colour accent)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 14);
    slider.setNumDecimalPlacesToDisplay (2);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setScrollWheelEnabled (true);
    slider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (slider);
    label.setText (title, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setFont (LucidLookAndFeel::font (10.5f));
    label.setColour (Label::textColourId, colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    attachment = std::make_unique<APVTS::SliderAttachment> (apvts, paramId, slider);
    if (auto* p = apvts.getParameter (paramId)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}
void Knob::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (13));
    slider.setBounds (b);
}

Choice::Choice (APVTS& apvts, const String& paramId, const String& title)
{
    // the attachment only synchronises the selection - the items come from the parameter
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (apvts.getParameter (paramId)))
        box.addItemList (choice->choices, 1);
    addAndMakeVisible (box);
    if (title.isNotEmpty())
    {
        label.setText (title, dontSendNotification);
        label.setFont (LucidLookAndFeel::font (10.5f));
        label.setColour (Label::textColourId, colours::textDim);
        label.setJustificationType (Justification::centred);
        addAndMakeVisible (label);
    }
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (apvts, paramId, box);
}
void Choice::resized()
{
    auto b = getLocalBounds();
    if (label.isVisible()) label.setBounds (b.removeFromTop (13));
    box.setBounds (b);
}

Switch::Switch (APVTS& apvts, const String& paramId, const String& text, Colour accent, bool titleAbove)
{
    button.setColour (ToggleButton::tickColourId, accent);
    addAndMakeVisible (button);
    if (titleAbove)
    {
        label.setText (text, dontSendNotification);
        label.setJustificationType (Justification::centred);
        label.setFont (LucidLookAndFeel::font (10.0f));
        label.setColour (Label::textColourId, colours::textDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    }
    else button.setButtonText (text);
    attachment = std::make_unique<APVTS::ButtonAttachment> (apvts, paramId, button);
}
void Switch::resized()
{
    auto b = getLocalBounds();
    if (label.isVisible())
    {
        label.setBounds (b.removeFromTop (13));
        button.setBounds (b.withSizeKeepingCentre (30, 18));
    }
    else button.setBounds (b);
}

Panel::Panel (const String& t, Colour a) : title (t), accent (a) {}
void Panel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    // accent tab + title
    auto ta = titleArea();
    g.setColour (accent);
    g.fillRoundedRectangle ((float) ta.getX(), (float) ta.getY() + 4.0f, 3.0f, (float) ta.getHeight() - 8.0f, 1.5f);
    g.setFont (LucidLookAndFeel::font (12.0f, true));
    g.setColour (colours::text);
    g.drawText (title, ta.withTrimmedLeft (10), Justification::centredLeft);
}

void layoutRow (Rectangle<int> row, std::initializer_list<Component*> comps, int gap)
{
    const int n = (int) comps.size(); if (n == 0) return;
    const int w = (row.getWidth() - gap * (n - 1)) / n;
    int x = row.getX();
    for (auto* c : comps) { c->setBounds (x, row.getY(), w, row.getHeight()); x += w + gap; }
}


} // namespace lucid
