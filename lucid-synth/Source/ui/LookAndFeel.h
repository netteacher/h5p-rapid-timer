// LUCID Synth - visual style: dark, clean, high-contrast, generous spacing.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace lucid {

namespace colours
{
    const juce::Colour background   { 0xff111317 };
    const juce::Colour panel        { 0xff1a1d23 };
    const juce::Colour panelLight   { 0xff22262e };
    const juce::Colour outline      { 0xff2c313b };
    const juce::Colour text         { 0xffe9ecf1 };
    const juce::Colour textDim      { 0xff8b93a5 };
    const juce::Colour accent       { 0xff45d0ff };   // cyan   - oscillator A / global
    const juce::Colour accentB      { 0xffffb347 };   // amber  - oscillator B
    const juce::Colour filter       { 0xffb58cff };   // violet - filters
    const juce::Colour envelope     { 0xff7ce7a8 };   // mint   - envelopes
    const juce::Colour lfo          { 0xff5ee0d0 };   // teal   - LFOs
    const juce::Colour fx           { 0xffff7a9e };   // pink   - effects
    const juce::Colour mod          { 0xfff5d76e };   // yellow - modulation matrix
    const juce::Colour red          { 0xffff5c5c };
}

class LucidLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LucidLookAndFeel();

    static juce::Font font (float size, bool bold = false);

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& bg, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text, const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;

    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int w, int h) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;
};

} // namespace lucid
