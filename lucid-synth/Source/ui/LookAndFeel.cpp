#include "LookAndFeel.h"

namespace lucid {

using namespace juce;

LucidLookAndFeel::LucidLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, colours::background);
    setColour (Slider::rotarySliderFillColourId, colours::accent);
    setColour (Slider::rotarySliderOutlineColourId, colours::outline);
    setColour (Slider::thumbColourId, colours::text);
    setColour (Slider::trackColourId, colours::accent);
    setColour (Slider::backgroundColourId, colours::outline);
    setColour (Slider::textBoxTextColourId, colours::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId, colours::text);
    setColour (ComboBox::backgroundColourId, colours::panelLight);
    setColour (ComboBox::textColourId, colours::text);
    setColour (ComboBox::outlineColourId, colours::outline);
    setColour (ComboBox::arrowColourId, colours::textDim);
    setColour (PopupMenu::backgroundColourId, colours::panelLight);
    setColour (PopupMenu::textColourId, colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, colours::text);
    setColour (TextButton::buttonColourId, colours::panelLight);
    setColour (TextButton::buttonOnColourId, colours::accent);
    setColour (TextButton::textColourOffId, colours::text);
    setColour (TextButton::textColourOnId, colours::background);
    setColour (ToggleButton::textColourId, colours::text);
    setColour (ToggleButton::tickColourId, colours::accent);
    setColour (TextEditor::backgroundColourId, colours::panelLight);
    setColour (TextEditor::textColourId, colours::text);
    setColour (TextEditor::outlineColourId, colours::outline);
    setColour (TextEditor::focusedOutlineColourId, colours::accent);
    setColour (TextEditor::highlightColourId, colours::accent.withAlpha (0.4f));
    setColour (AlertWindow::backgroundColourId, colours::panel);
    setColour (AlertWindow::textColourId, colours::text);
    setColour (AlertWindow::outlineColourId, colours::outline);
    setColour (TabbedButtonBar::tabOutlineColourId, Colours::transparentBlack);
    setColour (TabbedButtonBar::frontOutlineColourId, Colours::transparentBlack);
    setColour (TabbedComponent::backgroundColourId, Colours::transparentBlack);
    setColour (TabbedComponent::outlineColourId, Colours::transparentBlack);
    setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffe4e7ee));
    setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff1a1d23));
    setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff9aa0ad));
    setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha (0.35f));
    setColour (MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha (0.75f));
    setColour (MidiKeyboardComponent::textLabelColourId, colours::textDim);
    setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, colours::panelLight);
    setColour (MidiKeyboardComponent::upDownButtonArrowColourId, colours::text);
    setColour (ScrollBar::thumbColourId, colours::outline);
    setColour (TooltipWindow::backgroundColourId, colours::panelLight);
    setColour (TooltipWindow::textColourId, colours::text);
    setColour (TooltipWindow::outlineColourId, colours::outline);
}

Font LucidLookAndFeel::font (float size, bool bold)
{
    return Font (FontOptions (size, bold ? Font::bold : Font::plain));
}

// ----------------------------------------------------------------------------- sliders
void LucidLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    const auto area = Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const float radius = size * 0.5f;
    const float lineW = jmax (2.0f, size * 0.085f);
    const float arcR = radius - lineW * 0.5f;
    const auto fill = s.findColour (Slider::rotarySliderFillColourId);
    const auto track = s.findColour (Slider::rotarySliderOutlineColourId);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const bool enabled = s.isEnabled();

    // track
    Path back;
    back.addCentredArc (area.getCentreX(), area.getCentreY(), arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (track);
    g.strokePath (back, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));

    // value arc (from centre for bipolar)
    if (enabled)
    {
        Path val;
        const float from = bipolar ? startAngle + 0.5f * (endAngle - startAngle) : startAngle;
        if (std::abs (angle - from) > 0.001f)
        {
            val.addCentredArc (area.getCentreX(), area.getCentreY(), arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
            g.setColour (fill.withAlpha (0.25f));
            g.strokePath (val, PathStrokeType (lineW * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (fill);
            g.strokePath (val, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    // body
    const auto body = area.reduced (lineW * 1.9f);
    ColourGradient grad (colours::panelLight.brighter (0.18f), body.getX(), body.getY(), colours::panel.darker (0.2f), body.getX(), body.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (body);
    g.setColour (colours::outline.brighter (0.1f));
    g.drawEllipse (body, 1.0f);

    // pointer
    const float pr = body.getWidth() * 0.5f;
    Path pointer;
    pointer.addRoundedRectangle (-lineW * 0.45f, -pr + 3.0f, lineW * 0.9f, pr * 0.42f, lineW * 0.4f);
    pointer.applyTransform (AffineTransform::rotation (angle).translated (body.getCentreX(), body.getCentreY()));
    g.setColour (enabled ? fill : colours::textDim);
    g.fillPath (pointer);
}

void LucidLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float, float, Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearHorizontal && style != Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, s);
        return;
    }
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float cy = bounds.getCentreY();
    const float trackH = jmin (6.0f, bounds.getHeight() * 0.35f);
    const auto track = Rectangle<float> (bounds.getX(), cy - trackH * 0.5f, bounds.getWidth(), trackH);
    g.setColour (s.findColour (Slider::backgroundColourId));
    g.fillRoundedRectangle (track, trackH * 0.5f);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroX = bipolar ? bounds.getCentreX() : bounds.getX();
    const auto fillR = Rectangle<float> (jmin (zeroX, sliderPos), track.getY(), std::abs (sliderPos - zeroX), trackH);
    g.setColour (s.findColour (Slider::trackColourId).withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle (fillR, trackH * 0.5f);
    const float thumbR = jmin (8.0f, bounds.getHeight() * 0.45f);
    g.setColour (s.findColour (Slider::thumbColourId));
    g.fillEllipse (Rectangle<float> (thumbR * 2, thumbR * 2).withCentre ({ sliderPos, cy }));
    g.setColour (colours::background);
    g.drawEllipse (Rectangle<float> (thumbR * 2, thumbR * 2).withCentre ({ sliderPos, cy }), 1.0f);
}

Label* LucidLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (10.5f));
    l->setBorderSize (BorderSize<int> (0, 1, 0, 1));
    l->setJustificationType (Justification::centred);
    l->setColour (Label::textColourId, colours::textDim);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundWhenEditingColourId, colours::panelLight);
    l->setColour (Label::textWhenEditingColourId, colours::text);
    l->setColour (Label::outlineWhenEditingColourId, colours::accent);
    return l;
}

Slider::SliderLayout LucidLookAndFeel::getSliderLayout (Slider& s)
{
    Slider::SliderLayout layout;
    auto b = s.getLocalBounds();
    if (s.getSliderStyle() == Slider::RotaryHorizontalVerticalDrag || s.getSliderStyle() == Slider::Rotary)
    {
        if (s.getTextBoxPosition() == Slider::TextBoxBelow)
        {
            layout.textBoxBounds = b.removeFromBottom (14);
            layout.sliderBounds = b;
        }
        else { layout.sliderBounds = b; layout.textBoxBounds = {}; }
        return layout;
    }
    return LookAndFeel_V4::getSliderLayout (s);
}

// ----------------------------------------------------------------------------- buttons
void LucidLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto accent = b.findColour (ToggleButton::tickColourId);
    const float h = jmin (16.0f, bounds.getHeight() - 2.0f);
    const float w = h * 1.8f;
    const auto sw = Rectangle<float> (bounds.getX() + 1.0f, bounds.getCentreY() - h * 0.5f, w, h);
    g.setColour (on ? accent : colours::outline.brighter (highlighted ? 0.2f : 0.0f));
    g.fillRoundedRectangle (sw, h * 0.5f);
    const float kn = h - 4.0f;
    g.setColour (on ? colours::background : colours::textDim);
    g.fillEllipse (Rectangle<float> (kn, kn).withCentre ({ on ? sw.getRight() - h * 0.5f : sw.getX() + h * 0.5f, sw.getCentreY() }));
    if (b.getButtonText().isNotEmpty())
    {
        g.setColour (b.findColour (ToggleButton::textColourId).withAlpha (b.isEnabled() ? 1.0f : 0.5f));
        g.setFont (font (11.5f));
        g.drawText (b.getButtonText(), bounds.withTrimmedLeft (w + 7.0f).toNearestInt(), Justification::centredLeft);
    }
}

void LucidLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    Colour c = on ? b.findColour (TextButton::buttonOnColourId) : b.findColour (TextButton::buttonColourId);
    if (highlighted) c = c.brighter (0.12f);
    if (down) c = c.brighter (0.25f);
    g.setColour (c);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (on ? c.brighter (0.2f) : colours::outline);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
}

Font LucidLookAndFeel::getTextButtonFont (TextButton&, int h) { return font (jmin (12.5f, (float) h * 0.55f), false); }

// ----------------------------------------------------------------------------- combo box
void LucidLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto bounds = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::accent : box.findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    Path chevron;
    const float cx = (float) w - 10.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f); chevron.lineTo (cx, cy + 2.0f); chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (box.findColour (ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (chevron, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
}
Font LucidLookAndFeel::getComboBoxFont (ComboBox& b) { return font (jmin (12.0f, (float) b.getHeight() * 0.6f)); }
void LucidLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

// ----------------------------------------------------------------------------- popup menu
void LucidLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
    g.setColour (colours::outline.brighter (0.2f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 6.0f, 1.0f);
}
void LucidLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                          bool hasSubMenu, const String& text, const String& shortcut, const Drawable*, const Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colours::outline);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (3, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (colours::accent.withAlpha (0.22f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }
    g.setColour (textColour != nullptr ? *textColour : (isActive ? colours::text : colours::textDim));
    g.setFont (font (12.5f));
    auto tr = r.reduced (10, 0);
    if (isTicked)
    {
        g.setColour (colours::accent);
        g.fillEllipse (Rectangle<float> (5, 5).withCentre ({ (float) tr.getX() + 2.0f, (float) tr.getCentreY() }));
        g.setColour (isActive ? colours::text : colours::textDim);
    }
    g.drawText (text, tr.withTrimmedLeft (12), Justification::centredLeft, true);
    if (shortcut.isNotEmpty()) { g.setColour (colours::textDim); g.drawText (shortcut, tr, Justification::centredRight); }
    if (hasSubMenu)
    {
        Path p; const float cx = (float) tr.getRight() - 4.0f, cy = (float) tr.getCentreY();
        p.startNewSubPath (cx - 2.0f, cy - 3.5f); p.lineTo (cx + 1.5f, cy); p.lineTo (cx - 2.0f, cy + 3.5f);
        g.setColour (colours::textDim); g.strokePath (p, PathStrokeType (1.4f));
    }
}
Font LucidLookAndFeel::getPopupMenuFont() { return font (12.5f); }

// ----------------------------------------------------------------------------- tabs
void LucidLookAndFeel::drawTabButton (TabBarButton& b, Graphics& g, bool isMouseOver, bool)
{
    const auto area = b.getActiveArea().toFloat();
    const bool front = b.isFrontTab();
    static const Colour accents[] = { colours::mod, colours::fx, colours::accent, colours::filter };
    const auto accent = accents[jlimit (0, 3, b.getIndex())];
    if (front) { g.setColour (colours::panelLight); g.fillRoundedRectangle (area.reduced (2.0f, 3.0f), 5.0f); }
    else if (isMouseOver) { g.setColour (colours::panelLight.withAlpha (0.5f)); g.fillRoundedRectangle (area.reduced (2.0f, 3.0f), 5.0f); }
    g.setColour (front ? colours::text : colours::textDim);
    g.setFont (font (12.0f, front));
    g.drawText (b.getButtonText().toUpperCase(), area.toNearestInt(), Justification::centred);
    if (front) { g.setColour (accent); g.fillRoundedRectangle (area.getX() + 12.0f, area.getBottom() - 4.0f, area.getWidth() - 24.0f, 2.0f, 1.0f); }
}
int LucidLookAndFeel::getTabButtonBestWidth (TabBarButton& b, int) { return (int) font (12.0f, true).getStringWidthFloat (b.getButtonText()) + 40; }
void LucidLookAndFeel::drawTabAreaBehindFrontButton (TabbedButtonBar&, Graphics&, int, int) {}

// ----------------------------------------------------------------------------- labels
void LucidLookAndFeel::drawLabel (Graphics& g, Label& l)
{
    g.fillAll (l.findColour (Label::backgroundColourId));
    if (! l.isBeingEdited())
    {
        g.setColour (l.findColour (Label::textColourId).withMultipliedAlpha (l.isEnabled() ? 1.0f : 0.5f));
        g.setFont (l.getFont());
        g.drawFittedText (l.getText(), l.getBorderSize().subtractedFrom (l.getLocalBounds()), l.getJustificationType(), 1, 0.7f);
    }
}

} // namespace lucid
