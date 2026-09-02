#include "Panels.h"
#include <numeric>

namespace lucid {
using namespace juce;

// ============================================================================ Oscillator
OscPanel::OscPanel (LucidAudioProcessor& p, int oscIndex)
    : Panel (oscIndex == 0 ? "OSC A" : "OSC B", oscIndex == 0 ? colours::accent : colours::accentB),
      osc (oscIndex), c (oscIndex == 0 ? colours::accent : colours::accentB),
      on (p.apvts, ids::osc (osc, "on"), "", c),
      engine (p.apvts, ids::osc (osc, "engine")), table (p.apvts, ids::osc (osc, "table")), shape (p.apvts, ids::osc (osc, "shape")),
      display (p, osc, c),
      morph (p.apvts, ids::osc (osc, "morph"), "MORPH", c), pw (p.apvts, ids::osc (osc, "pw"), "PULSE W", c),
      level (p.apvts, ids::osc (osc, "level"), "LEVEL", c), pan (p.apvts, ids::osc (osc, "pan"), "PAN", c),
      oct (p.apvts, ids::osc (osc, "oct"), "OCT", c), semi (p.apvts, ids::osc (osc, "semi"), "SEMI", c), fine (p.apvts, ids::osc (osc, "fine"), "FINE", c),
      drift (p.apvts, ids::osc (osc, "drift"), "DRIFT", c),
      unison (p.apvts, ids::osc (osc, "unison"), "UNISON", c), detune (p.apvts, ids::osc (osc, "detune"), "DETUNE", c),
      spread (p.apvts, ids::osc (osc, "spread"), "SPREAD", c), blend (p.apvts, ids::osc (osc, "blend"), "BLEND", c),
      phase (p.apvts, ids::osc (osc, "phase"), "PHASE", c),
      retrig (p.apvts, ids::osc (osc, "retrig"), "RETRIG", c, true)
{
    for (auto* comp : std::initializer_list<Component*> { &on, &engine, &table, &shape, &display, &morph, &pw, &level, &pan, &oct, &semi, &fine, &drift, &unison, &detune, &spread, &blend, &phase, &retrig })
        addAndMakeVisible (comp);
    engineValue = p.apvts.getRawParameterValue (ids::osc (osc, "engine"));
    refresh();
}
void OscPanel::refresh()
{
    display.refresh();
    const int e = (int) engineValue->load();
    if (e != lastEngine)
    {
        lastEngine = e;
        const bool analog = e == 1;
        table.setVisible (! analog); shape.setVisible (analog);
        morph.setVisible (! analog); pw.setVisible (analog);
        drift.setEnabled (true);
        resized();
    }
}
void OscPanel::resized()
{
    auto ta = titleArea();
    ta.removeFromLeft (64);
    on.setBounds (ta.removeFromLeft (34));
    ta.removeFromLeft (4);
    engine.setBounds (ta.removeFromLeft (100).reduced (0, 1));
    ta.removeFromLeft (4);
    table.setBounds (ta.reduced (0, 1)); shape.setBounds (ta.reduced (0, 1));

    auto a = content();
    auto top = a.removeFromTop (108);
    display.setBounds (top.removeFromLeft (top.getWidth() - 150));
    top.removeFromLeft (6);
    const int bigKnob = top.getWidth() / 2;
    auto k1 = top.removeFromLeft (bigKnob);
    morph.setBounds (k1); pw.setBounds (k1);
    level.setBounds (top);
    a.removeFromTop (4);
    const int rowH = (a.getHeight() - 4) / 2;
    layoutRow (a.removeFromTop (rowH), { &oct, &semi, &fine, &pan, &drift, &retrig });
    a.removeFromTop (4);
    layoutRow (a, { &unison, &detune, &spread, &blend, &phase });
}

// ============================================================================ Filter
FilterPanel::Unit::Unit (LucidAudioProcessor& p, int i)
    : on (p.apvts, ids::filt (i, "on"), "", colours::filter), type (p.apvts, ids::filt (i, "type")),
      cutoff (p.apvts, ids::filt (i, "cutoff"), "CUTOFF", colours::filter), res (p.apvts, ids::filt (i, "res"), "RESO", colours::filter),
      drive (p.apvts, ids::filt (i, "drive"), "DRIVE", colours::filter), env (p.apvts, ids::filt (i, "env"), "ENV 2", colours::filter),
      key (p.apvts, ids::filt (i, "key"), "KEY", colours::filter) {}

FilterPanel::FilterPanel (LucidAudioProcessor& p)
    : Panel ("FILTERS", colours::filter), display (p),
      routing (p.apvts, "routing", "ROUTING"), mix (p.apvts, "filterMix", "MIX", colours::filter)
{
    addAndMakeVisible (display); addAndMakeVisible (routing); addAndMakeVisible (mix);
    for (int i = 0; i < kNumFilters; ++i)
    {
        auto u = std::make_unique<Unit> (p, i);
        u->title.setText ("F" + String (i + 1), dontSendNotification);
        u->title.setFont (LucidLookAndFeel::font (11.5f, true));
        u->title.setColour (Label::textColourId, colours::filter);
        for (auto* comp : std::initializer_list<Component*> { &u->on, &u->type, &u->cutoff, &u->res, &u->drive, &u->env, &u->key, &u->title }) addAndMakeVisible (comp);
        units[i] = std::move (u);
    }
}
void FilterPanel::resized()
{
    auto a = content();
    display.setBounds (a.removeFromTop (100));
    a.removeFromTop (4);
    const int rowH = a.getHeight() / 2;
    for (int i = 0; i < kNumFilters; ++i)
    {
        auto row = a.removeFromTop (rowH);
        auto head = row.removeFromLeft (20);
        units[i]->title.setBounds (head.withSizeKeepingCentre (20, 20));
        units[i]->on.setBounds (row.removeFromLeft (30).withSizeKeepingCentre (30, 20));
        row.removeFromLeft (2);
        units[i]->type.setBounds (row.removeFromLeft (74).withSizeKeepingCentre (74, 22));
        row.removeFromLeft (4);
        auto extra = row.removeFromRight (56);
        if (i == 0) routing.setBounds (extra.withSizeKeepingCentre (56, 36));
        else mix.setBounds (extra);
        row.removeFromRight (4);
        layoutRow (row, { &units[i]->cutoff, &units[i]->res, &units[i]->drive, &units[i]->env, &units[i]->key }, 2);
    }
}

// ============================================================================ Envelopes
EnvPanel::Unit::Unit (LucidAudioProcessor& p, int i)
    : display (p, i, colours::envelope),
      a (p.apvts, ids::env (i, "a"), "ATK", colours::envelope), d (p.apvts, ids::env (i, "d"), "DEC", colours::envelope),
      s (p.apvts, ids::env (i, "s"), "SUS", colours::envelope), r (p.apvts, ids::env (i, "r"), "REL", colours::envelope),
      curve (p.apvts, ids::env (i, "curve"), "CURVE", colours::envelope), vel (p.apvts, ids::env (i, "vel"), "VEL", colours::envelope) {}

EnvPanel::EnvPanel (LucidAudioProcessor& p) : Panel ("ENVELOPES", colours::envelope)
{
    static const char* names[kNumEnvs] = { "ENV 1 - AMP", "ENV 2 - FILTER", "ENV 3 - MOD" };
    for (int i = 0; i < kNumEnvs; ++i)
    {
        auto u = std::make_unique<Unit> (p, i);
        u->title.setText (names[i], dontSendNotification);
        u->title.setFont (LucidLookAndFeel::font (10.5f, true));
        u->title.setColour (Label::textColourId, colours::textDim);
        for (auto* comp : std::initializer_list<Component*> { &u->title, &u->display, &u->a, &u->d, &u->s, &u->r, &u->curve, &u->vel }) addAndMakeVisible (comp);
        units[i] = std::move (u);
    }
}
void EnvPanel::resized()
{
    auto a = content();
    const int w = (a.getWidth() - 12) / kNumEnvs;
    for (int i = 0; i < kNumEnvs; ++i)
    {
        auto col = a.removeFromLeft (w); a.removeFromLeft (6);
        units[i]->title.setBounds (col.removeFromTop (14));
        units[i]->display.setBounds (col.removeFromTop (60));
        col.removeFromTop (4);
        auto row1 = col.removeFromTop (58);
        layoutRow (row1, { &units[i]->a, &units[i]->d, &units[i]->s, &units[i]->r }, 2);
        auto row2 = col.withTrimmedTop (2);
        layoutRow (row2.withWidth (row2.getWidth() / 2), { &units[i]->curve, &units[i]->vel }, 2);
    }
}

// ============================================================================ LFOs
LfoPanel::Unit::Unit (LucidAudioProcessor& p, int i)
    : shape (p.apvts, ids::lfo (i, "shape")), display (p, i, colours::lfo),
      rate (p.apvts, ids::lfo (i, "rate"), "RATE", colours::lfo), phase (p.apvts, ids::lfo (i, "phase"), "PHASE", colours::lfo),
      fade (p.apvts, ids::lfo (i, "fade"), "FADE", colours::lfo), smooth (p.apvts, ids::lfo (i, "smooth"), "SMTH", colours::lfo),
      sync (p.apvts, ids::lfo (i, "sync"), "SYNC", colours::lfo, true), mono (p.apvts, ids::lfo (i, "mono"), "MONO", colours::lfo, true), div (p.apvts, ids::lfo (i, "div"), "DIV") {}

LfoPanel::LfoPanel (LucidAudioProcessor& p) : Panel ("LFOS", colours::lfo)
{
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto u = std::make_unique<Unit> (p, i);
        u->title.setText ("LFO " + String (i + 1), dontSendNotification);
        u->title.setFont (LucidLookAndFeel::font (10.5f, true));
        u->title.setColour (Label::textColourId, colours::textDim);
        for (auto* comp : std::initializer_list<Component*> { &u->title, &u->shape, &u->display, &u->rate, &u->phase, &u->fade, &u->smooth, &u->sync, &u->mono, &u->div }) addAndMakeVisible (comp);
        units[i] = std::move (u);
    }
}
void LfoPanel::resized()
{
    auto a = content();
    const int w = (a.getWidth() - 12) / kNumLfos;
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto col = a.removeFromLeft (w); a.removeFromLeft (6);
        auto head = col.removeFromTop (18);
        units[i]->title.setBounds (head.removeFromLeft (40));
        units[i]->shape.setBounds (head.reduced (0, 0));
        col.removeFromTop (3);
        units[i]->display.setBounds (col.removeFromTop (54));
        col.removeFromTop (3);
        auto row1 = col.removeFromTop (58);
        layoutRow (row1, { &units[i]->rate, &units[i]->phase, &units[i]->fade, &units[i]->smooth }, 2);
        auto row2 = col.withTrimmedTop (2).withHeight (36);
        units[i]->sync.setBounds (row2.removeFromLeft (42));
        row2.removeFromLeft (4);
        units[i]->div.setBounds (row2.removeFromLeft (72));
        row2.removeFromLeft (4);
        units[i]->mono.setBounds (row2.removeFromLeft (42));
    }
}

// ============================================================================ Mod matrix
ModMatrixPanel::Row::Row (LucidAudioProcessor& p, int i) : src (p.apvts, ids::mod (i, "src")), dst (p.apvts, ids::mod (i, "dst")) {}

ModMatrixPanel::ModMatrixPanel (LucidAudioProcessor& p)
{
    for (int i = 0; i < kNumModSlots; ++i)
    {
        auto r = std::make_unique<Row> (p, i);
        r->index.setText (String (i + 1), dontSendNotification);
        r->index.setFont (LucidLookAndFeel::font (10.5f, true));
        r->index.setColour (Label::textColourId, colours::textDim);
        r->index.setJustificationType (Justification::centred);
        r->amount.setSliderStyle (Slider::LinearHorizontal);
        r->amount.setTextBoxStyle (Slider::TextBoxRight, false, 44, 16);
        r->amount.setColour (Slider::trackColourId, colours::mod);
        r->amount.setDoubleClickReturnValue (true, 0.0);
        r->att = std::make_unique<APVTS::SliderAttachment> (p.apvts, ids::mod (i, "amt"), r->amount);
        addAndMakeVisible (r->index); addAndMakeVisible (r->src); addAndMakeVisible (r->dst); addAndMakeVisible (r->amount);
        rows[i] = std::move (r);
    }
}
void ModMatrixPanel::paint (Graphics& g)
{
    g.setFont (LucidLookAndFeel::font (9.5f, true)); g.setColour (colours::textDim);
    auto a = getLocalBounds().reduced (6);
    const int colW = (a.getWidth() - 12) / 2;
    for (int c = 0; c < 2; ++c)
    {
        auto col = a.withX (a.getX() + c * (colW + 12)).withWidth (colW);
        auto head = col.removeFromTop (14);
        head.removeFromLeft (20);
        g.drawText ("SOURCE", head.removeFromLeft (colW / 3), Justification::centredLeft);
        g.drawText ("DESTINATION", head.removeFromLeft (colW / 3 + 10), Justification::centredLeft);
        g.drawText ("AMOUNT", head, Justification::centredLeft);
    }
}
void ModMatrixPanel::resized()
{
    auto a = getLocalBounds().reduced (6);
    a.removeFromTop (14);
    const int colW = (a.getWidth() - 12) / 2;
    const int rowsPerCol = kNumModSlots / 2;
    const int rowH = jmin (24, a.getHeight() / rowsPerCol);
    for (int i = 0; i < kNumModSlots; ++i)
    {
        const int c = i / rowsPerCol, rIdx = i % rowsPerCol;
        auto row = Rectangle<int> (a.getX() + c * (colW + 12), a.getY() + rIdx * rowH, colW, rowH).reduced (0, 2);
        rows[i]->index.setBounds (row.removeFromLeft (20));
        rows[i]->src.setBounds (row.removeFromLeft (colW / 3).reduced (2, 0));
        rows[i]->dst.setBounds (row.removeFromLeft (colW / 3 + 10).reduced (2, 0));
        rows[i]->amount.setBounds (row);
    }
}

// ============================================================================ Effects
FxPanel::FxPanel (LucidAudioProcessor& p) : proc (p)
{
    auto& v = p.apvts;
    const auto c = colours::fx;
    auto add = [&] (const String& name, int width, const String& onId, std::vector<std::unique_ptr<Component>> items)
    {
        auto s = std::make_unique<Section> (name, width);
        s->on = std::make_unique<Switch> (v, onId, "", c);
        addAndMakeVisible (*s->on);
        for (auto& it : items) addAndMakeVisible (*it);
        s->items = std::move (items);
        sections.push_back (std::move (s));
    };
    auto K = [&] (const char* id, const char* t) -> std::unique_ptr<Component> { return std::make_unique<Knob> (v, id, t, c); };
    auto C = [&] (const char* id, const char* t) -> std::unique_ptr<Component> { return std::make_unique<Choice> (v, id, t); };
    auto S = [&] (const char* id, const char* t) -> std::unique_ptr<Component> { return std::make_unique<Switch> (v, id, t, c, true); };

    std::vector<std::unique_ptr<Component>> sat; sat.push_back (C ("sat_type", "TYPE")); sat.push_back (K ("sat_drive", "DRIVE")); sat.push_back (K ("sat_mix", "MIX"));
    add ("SATURATOR", 150, "sat_on", std::move (sat));
    std::vector<std::unique_ptr<Component>> eq;
    eq.push_back (K ("eq_lowFreq", "LOW HZ")); eq.push_back (K ("eq_lowGain", "LOW DB")); eq.push_back (K ("eq_midFreq", "MID HZ")); eq.push_back (K ("eq_midGain", "MID DB"));
    eq.push_back (K ("eq_midQ", "MID Q")); eq.push_back (K ("eq_highFreq", "HIGH HZ")); eq.push_back (K ("eq_highGain", "HIGH DB"));
    add ("EQ", 200, "eq_on", std::move (eq));
    std::vector<std::unique_ptr<Component>> comp;
    comp.push_back (K ("comp_thresh", "THRESH")); comp.push_back (K ("comp_ratio", "RATIO")); comp.push_back (K ("comp_attack", "ATTACK"));
    comp.push_back (K ("comp_release", "RELEASE")); comp.push_back (K ("comp_makeup", "MAKEUP")); comp.push_back (K ("comp_mix", "MIX"));
    add ("COMPRESSOR", 150, "comp_on", std::move (comp));
    std::vector<std::unique_ptr<Component>> cho;
    cho.push_back (K ("cho_rate", "RATE")); cho.push_back (K ("cho_depth", "DEPTH")); cho.push_back (K ("cho_mix", "MIX")); cho.push_back (K ("cho_width", "WIDTH"));
    add ("CHORUS", 150, "cho_on", std::move (cho));
    std::vector<std::unique_ptr<Component>> dly;
    dly.push_back (S ("dly_sync", "SYNC")); dly.push_back (C ("dly_div", "DIV")); dly.push_back (K ("dly_time", "TIME")); dly.push_back (K ("dly_fb", "FEEDBACK"));
    dly.push_back (K ("dly_mix", "MIX")); dly.push_back (S ("dly_pingpong", "PING PONG")); dly.push_back (K ("dly_lowcut", "LOW CUT")); dly.push_back (K ("dly_highcut", "HIGH CUT"));
    add ("DELAY", 200, "dly_on", std::move (dly));
    std::vector<std::unique_ptr<Component>> rev;
    rev.push_back (K ("rev_size", "SIZE")); rev.push_back (K ("rev_decay", "DECAY")); rev.push_back (K ("rev_damp", "DAMP")); rev.push_back (K ("rev_predelay", "PRE"));
    rev.push_back (K ("rev_mix", "MIX")); rev.push_back (K ("rev_width", "WIDTH"));
    add ("REVERB", 150, "rev_on", std::move (rev));
    std::vector<std::unique_ptr<Component>> lim; lim.push_back (K ("lim_ceiling", "CEILING"));
    add ("LIMITER", 96, "lim_on", std::move (lim));
}
void FxPanel::paint (Graphics& g)
{
    auto a = getLocalBounds().reduced (4);
    int x = a.getX();
    const int total = std::accumulate (sections.begin(), sections.end(), 0, [] (int s, const std::unique_ptr<Section>& sec) { return s + sec->width; });
    const float scale = (float) (a.getWidth() - 6 * ((int) sections.size() - 1)) / (float) total;
    for (auto& s : sections)
    {
        const int w = (int) ((float) s->width * scale);
        auto r = Rectangle<int> (x, a.getY(), w, a.getHeight());
        g.setColour (colours::panelLight.withAlpha (0.55f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setFont (LucidLookAndFeel::font (10.5f, true));
        g.setColour (s->on->button.getToggleState() ? colours::fx : colours::textDim);
        g.drawText (s->name, r.reduced (8, 4).withTrimmedLeft (34).withHeight (18), Justification::centredLeft);
        x += w + 6;
    }
}
void FxPanel::resized()
{
    auto a = getLocalBounds().reduced (4);
    int x = a.getX();
    const int total = std::accumulate (sections.begin(), sections.end(), 0, [] (int s, const std::unique_ptr<Section>& sec) { return s + sec->width; });
    const float scale = (float) (a.getWidth() - 6 * ((int) sections.size() - 1)) / (float) total;
    for (auto& s : sections)
    {
        const int w = (int) ((float) s->width * scale);
        auto r = Rectangle<int> (x, a.getY(), w, a.getHeight()).reduced (6, 4);
        s->on->setBounds (r.removeFromTop (18).removeFromLeft (30));
        r.removeFromTop (2);
        // grid of 46 px cells; combo boxes span two cells
        const int perRow = jmax (2, r.getWidth() / 46);
        int totalCells = 0;
        for (auto& it : s->items) totalCells += dynamic_cast<Choice*> (it.get()) != nullptr ? 2 : 1;
        const int nRows = jmax (1, (totalCells + perRow - 1) / perRow);
        const int rowH = r.getHeight() / nRows;
        const int cellW = r.getWidth() / perRow;
        int cell = 0;
        for (auto& it : s->items)
        {
            auto* comp = it.get();
            const int span = dynamic_cast<Choice*> (comp) != nullptr ? 2 : 1;
            if (cell % perRow + span > perRow) cell += perRow - cell % perRow; // wrap
            auto bounds = Rectangle<int> (r.getX() + (cell % perRow) * cellW, r.getY() + (cell / perRow) * rowH, cellW * span, rowH).reduced (1, 0);
            if (dynamic_cast<Switch*> (comp) != nullptr || span == 2) bounds = bounds.withSizeKeepingCentre (bounds.getWidth(), 36);
            comp->setBounds (bounds);
            cell += span;
        }
        x += w + 6;
    }
    repaint();
}

// ============================================================================ Global / mixer
GlobalPanel::GlobalPanel (LucidAudioProcessor& p)
    : subLevel (p.apvts, "sub_level", "LEVEL", colours::accent), noiseLevel (p.apvts, "noise_level", "LEVEL", colours::accent),
      noiseColor (p.apvts, "noise_color", "COLOUR", colours::accent), fm (p.apvts, "fm", "FM B>A", colours::accentB), ring (p.apvts, "ring", "RING", colours::accentB),
      glide (p.apvts, "glide", "GLIDE", colours::accent), velAmp (p.apvts, "velAmp", "VEL>AMP", colours::accent), pan (p.apvts, "pan", "PAN", colours::accent),
      subOct (p.apvts, "sub_oct", "OCTAVE"), subShape (p.apvts, "sub_shape", "SHAPE"), voiceMode (p.apvts, "voiceMode", "MODE"),
      voices (p.apvts, "voices", "VOICES", colours::accent), bendRange (p.apvts, "bendRange", "BEND", colours::accent)
{
    for (auto* l : { &subTitle, &noiseTitle, &mixTitle, &voiceTitle })
    {
        l->setFont (LucidLookAndFeel::font (10.5f, true)); l->setColour (Label::textColourId, colours::textDim); addAndMakeVisible (l);
    }
    subTitle.setText ("SUB OSC", dontSendNotification); noiseTitle.setText ("NOISE", dontSendNotification);
    mixTitle.setText ("OSC INTERACTION", dontSendNotification); voiceTitle.setText ("VOICING", dontSendNotification);
    for (auto* comp : std::initializer_list<Component*> { &subLevel, &noiseLevel, &noiseColor, &fm, &ring, &glide, &velAmp, &pan, &subOct, &subShape, &voiceMode, &voices, &bendRange })
        addAndMakeVisible (comp);
}
void GlobalPanel::paint (Graphics& g)
{
    auto a = getLocalBounds().reduced (4);
    const int w = (a.getWidth() - 18) / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto r = Rectangle<int> (a.getX() + i * (w + 6), a.getY(), w, a.getHeight());
        g.setColour (colours::panelLight.withAlpha (0.55f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
    }
}
void GlobalPanel::resized()
{
    auto a = getLocalBounds().reduced (4);
    const int w = (a.getWidth() - 18) / 4;
    auto col = [&] (int i) { return Rectangle<int> (a.getX() + i * (w + 6), a.getY(), w, a.getHeight()).reduced (8, 4); };
    auto c0 = col (0); subTitle.setBounds (c0.removeFromTop (18)); c0.removeFromTop (2);
    { auto row = c0.removeFromTop (60); layoutRow (row, { &subLevel, &subOct, &subShape }, 4); subOct.setBounds (subOct.getBounds().withSizeKeepingCentre (subOct.getWidth(), 36)); subShape.setBounds (subShape.getBounds().withSizeKeepingCentre (subShape.getWidth(), 36)); }
    auto c1 = col (1); noiseTitle.setBounds (c1.removeFromTop (18)); c1.removeFromTop (2);
    layoutRow (c1.removeFromTop (60), { &noiseLevel, &noiseColor }, 4);
    auto c2 = col (2); mixTitle.setBounds (c2.removeFromTop (18)); c2.removeFromTop (2);
    layoutRow (c2.removeFromTop (60), { &fm, &ring, &pan }, 4);
    auto c3 = col (3); voiceTitle.setBounds (c3.removeFromTop (18)); c3.removeFromTop (2);
    { auto row = c3.removeFromTop (60); layoutRow (row, { &voiceMode, &voices, &glide, &bendRange, &velAmp }, 4); voiceMode.setBounds (voiceMode.getBounds().withSizeKeepingCentre (voiceMode.getWidth(), 36)); }
}

// ============================================================================ Header
HeaderBar::HeaderBar (LucidAudioProcessor& p)
    : proc (p), master (p.apvts, "master", "MASTER", colours::accent), scope (p)
{
    for (int i = 0; i < kNumMacros; ++i)
    {
        macros[i] = std::make_unique<Knob> (p.apvts, ids::macro (i), "MACRO " + String (i + 1), colours::mod);
        addAndMakeVisible (*macros[i]);
    }
    addAndMakeVisible (master); addAndMakeVisible (scope);
    for (auto* b : { &prevButton, &nextButton, &presetButton, &saveButton }) addAndMakeVisible (b);
    prevButton.onClick = [this] { proc.presets.loadPrevious(); };
    nextButton.onClick = [this] { proc.presets.loadNext(); };
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton.onClick = [this] { savePreset(); };
    categoryLabel.setFont (LucidLookAndFeel::font (10.0f)); categoryLabel.setColour (Label::textColourId, colours::textDim);
    categoryLabel.setJustificationType (Justification::centred);
    voicesLabel.setFont (LucidLookAndFeel::font (10.0f)); voicesLabel.setColour (Label::textColourId, colours::textDim);
    voicesLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (categoryLabel); addAndMakeVisible (voicesLabel);
    proc.presets.onPresetChanged = [this] { updatePresetName(); };
    updatePresetName();
}
void HeaderBar::updatePresetName()
{
    presetButton.setButtonText (proc.presets.getCurrentName());
    const int idx = proc.presets.getCurrentIndex();
    categoryLabel.setText (proc.presets.getCategory (idx).toUpperCase(), dontSendNotification);
}
void HeaderBar::refresh()
{
    scope.refresh();
    voicesLabel.setText (String (proc.activeVoices.load()) + " voices", dontSendNotification);
}
void HeaderBar::showPresetMenu()
{
    PopupMenu menu;
    auto& pm = proc.presets;
    StringArray cats;
    for (int i = 0; i < pm.getNumPresets(); ++i) cats.addIfNotAlreadyThere (pm.getCategory (i));
    for (const auto& cat : cats)
    {
        PopupMenu sub;
        for (int i = 0; i < pm.getNumPresets(); ++i)
            if (pm.getCategory (i) == cat) sub.addItem (i + 1, pm.getName (i), true, i == pm.getCurrentIndex());
        menu.addSubMenu (cat, sub);
    }
    menu.addSeparator();
    menu.addItem (10001, "Open user preset folder...");
    menu.addItem (10002, "Rescan user presets");
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (presetButton).withMinimumWidth (200), [this] (int r)
    {
        if (r == 10001) { PresetManager::getUserPresetFolder().createDirectory(); PresetManager::getUserPresetFolder().revealToUser(); }
        else if (r == 10002) { proc.presets.rescanUserPresets(); updatePresetName(); }
        else if (r > 0) proc.presets.loadPreset (r - 1);
    });
}
void HeaderBar::savePreset()
{
    auto* w = new AlertWindow ("Save Preset", "Name for the preset (stored in Documents/LUCID/Presets):", MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.presets.getCurrentName(), "Name");
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1) proc.presets.saveUserPreset (w->getTextEditorContents ("name"));
        updatePresetName();
    }), true);
}
void HeaderBar::paint (Graphics& g)
{
    auto r = getLocalBounds();
    // logo
    auto logo = r.removeFromLeft (150).reduced (10, 8);
    g.setFont (LucidLookAndFeel::font (24.0f, true));
    g.setGradientFill (ColourGradient (colours::accent, (float) logo.getX(), 0, colours::filter, (float) logo.getRight(), 0, false));
    g.drawText ("LUCID", logo.removeFromTop (28), Justification::centredLeft);
    g.setFont (LucidLookAndFeel::font (9.0f));
    g.setColour (colours::textDim);
    g.drawText ("WAVETABLE SYNTHESIZER", logo, Justification::centredLeft);
    // divider under header
    g.setColour (colours::outline);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
}
void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromLeft (150);
    // preset browser
    auto pb = r.removeFromLeft (330).withSizeKeepingCentre (330, 40);
    auto row = pb.removeFromTop (24);
    prevButton.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (3);
    saveButton.setBounds (row.removeFromRight (48));
    row.removeFromRight (3);
    nextButton.setBounds (row.removeFromRight (28));
    row.removeFromRight (3);
    presetButton.setBounds (row);
    categoryLabel.setBounds (pb);
    r.removeFromLeft (18);
    // macros
    for (int i = 0; i < kNumMacros; ++i) { macros[i]->setBounds (r.removeFromLeft (58)); r.removeFromLeft (2); }
    r.removeFromLeft (14);
    // master + scope on the right
    master.setBounds (r.removeFromRight (62));
    r.removeFromRight (8);
    scope.setBounds (r.removeFromRight (190).reduced (0, 2));
    r.removeFromRight (8);
    voicesLabel.setBounds (r.removeFromRight (70));
}

} // namespace lucid
