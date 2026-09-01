#include "Displays.h"
#include "../dsp/Filters.h"

namespace lucid {
using namespace juce;

static void drawDisplayBackground (Graphics& g, Rectangle<float> r)
{
    g.setColour (colours::background);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::outline.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

// ============================================================================ Wavetable
WavetableDisplay::WavetableDisplay (LucidAudioProcessor& p, int oscIndex, Colour accent) : proc (p), osc (oscIndex), colour (accent)
{
    table = p.apvts.getRawParameterValue (ids::osc (osc, "table"));
    morph = p.apvts.getRawParameterValue (ids::osc (osc, "morph"));
    engine = p.apvts.getRawParameterValue (ids::osc (osc, "engine"));
    shape = p.apvts.getRawParameterValue (ids::osc (osc, "shape"));
    pw = p.apvts.getRawParameterValue (ids::osc (osc, "pw"));
    on = p.apvts.getRawParameterValue (ids::osc (osc, "on"));
    setInterceptsMouseClicks (false, false);
}

void WavetableDisplay::refresh()
{
    const float m = morph->load(); const int t = (int) table->load(); const int e = (int) engine->load(); const int sh = (int) shape->load(); const float w = pw->load();
    if (m != lastMorph || t != lastTable || e != lastEngine || sh != lastShape || w != lastPw)
    {
        lastMorph = m; lastTable = t; lastEngine = e; lastShape = sh; lastPw = w;
        repaint();
    }
}

void WavetableDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    const bool enabled = on->load() >= 0.5f;
    const auto c = enabled ? colour : colours::textDim;
    const auto& bank = proc.getEngine().getWavetables();
    const bool analog = (int) engine->load() == 1;
    const int shapeIdx = (int) shape->load();
    const float pwv = pw->load();
    const float morphV = morph->load();
    const int tableIdx = (int) table->load();

    // Sample generator: returns waveform value for phase 0..1 at a given frame morph
    auto sampleAt = [&] (float ph, float m) -> float
    {
        if (analog)
        {
            switch (shapeIdx)
            {
                case 0: return 2.0f * ph - 1.0f;
                case 1: return ph < pwv ? 1.0f : -1.0f;
                case 2: return 1.0f - 4.0f * std::abs (ph - 0.5f);
                default: return std::sin (kTwoPi * ph);
            }
        }
        if (! bank.isBuilt()) return std::sin (kTwoPi * ph);
        const auto& wt = bank.get (tableIdx);
        return wt.read (ph, m, 0.0f);
    };

    const auto area = r.reduced (10.0f, 8.0f);
    const int layers = analog ? 1 : 6;
    const float depthX = 8.0f, depthY = 7.0f;
    const float waveH = area.getHeight() - (float) (layers - 1) * depthY;
    const int N = 160;

    // ghost frames (behind), from far to near
    for (int layer = layers - 1; layer >= 0; --layer)
    {
        const float m = analog ? morphV : jlimit (0.0f, 1.0f, morphV + (float) layer * 0.12f);
        const float ox = (float) layer * depthX, oy = -(float) layer * depthY;
        const auto box = Rectangle<float> (area.getX() + ox, area.getBottom() - waveH + oy, area.getWidth() - (float) (layers - 1) * depthX, waveH);
        Path path;
        for (int i = 0; i <= N; ++i)
        {
            const float ph = (float) i / (float) N;
            const float v = jlimit (-1.0f, 1.0f, sampleAt (ph >= 1.0f ? 0.9999f : ph, m));
            const float x = box.getX() + ph * box.getWidth();
            const float y = box.getCentreY() - v * box.getHeight() * 0.46f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        if (layer == 0)
        {
            Path fill (path);
            fill.lineTo (box.getRight(), box.getCentreY()); fill.lineTo (box.getX(), box.getCentreY()); fill.closeSubPath();
            g.setGradientFill (ColourGradient (c.withAlpha (0.28f), 0, box.getY(), c.withAlpha (0.02f), 0, box.getBottom(), false));
            g.fillPath (fill);
            g.setColour (c.withAlpha (0.35f));
            g.strokePath (path, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (c);
            g.strokePath (path, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        else
        {
            g.setColour (c.withAlpha (0.28f - (float) layer * 0.04f));
            g.strokePath (path, PathStrokeType (1.0f));
        }
    }

    // caption
    g.setFont (LucidLookAndFeel::font (10.5f));
    g.setColour (colours::textDim);
    String caption = analog ? String ("ANALOG") : String (WavetableBank::tableName (tableIdx)).toUpperCase();
    if (! analog && bank.isBuilt())
        caption += "   FRAME " + String (1 + roundToInt (morphV * (float) (bank.get (tableIdx).numFrames - 1))) + "/" + String (bank.get (tableIdx).numFrames);
    g.drawText (caption, r.reduced (8.0f, 4.0f).toNearestInt(), Justification::bottomLeft);
}

// ============================================================================ Filter
FilterDisplay::FilterDisplay (LucidAudioProcessor& p) : proc (p)
{
    for (int i = 0; i < kNumFilters; ++i)
    {
        f[i].on = p.apvts.getRawParameterValue (ids::filt (i, "on"));
        f[i].type = p.apvts.getRawParameterValue (ids::filt (i, "type"));
        f[i].cutoff = p.apvts.getRawParameterValue (ids::filt (i, "cutoff"));
        f[i].res = p.apvts.getRawParameterValue (ids::filt (i, "res"));
        f[i].cutoffParam = p.apvts.getParameter (ids::filt (i, "cutoff"));
        f[i].resParam = p.apvts.getParameter (ids::filt (i, "res"));
    }
    smoothed.fill (-100.0f);
}

float FilterDisplay::xToHz (float x) const
{
    const float w = (float) getWidth() - 16.0f;
    const float n = jlimit (0.0f, 1.0f, (x - 8.0f) / w);
    return 20.0f * std::pow (1000.0f, n);
}
float FilterDisplay::hzToX (float hz) const
{
    const float w = (float) getWidth() - 16.0f;
    return 8.0f + w * (std::log (jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f));
}
float FilterDisplay::dbToY (float db) const
{
    const float h = (float) getHeight() - 16.0f;
    return 8.0f + h * (1.0f - jlimit (0.0f, 1.0f, (db + 48.0f) / 72.0f)); // -48 .. +24 dB
}
Point<float> FilterDisplay::handlePos (int i) const
{
    const float hz = f[i].cutoff->load();
    const float db = Filter::magnitudeDb ((FilterType) (int) f[i].type->load(), hz, f[i].res->load(), hz, (float) proc.getCurrentSampleRate());
    return { hzToX (hz), dbToY (db) };
}

void FilterDisplay::refresh()
{
    // spectrum of the recent output
    proc.scopeFeed.readLatest (fftData.data(), 2048);
    std::fill (fftData.begin() + 2048, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), 2048);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
    const float sr = (float) proc.getCurrentSampleRate();
    for (int i = 0; i < (int) smoothed.size(); ++i)
    {
        const float n = (float) i / (float) smoothed.size();
        const float hz = 20.0f * std::pow (1000.0f, n);
        const float bin = hz / sr * 2048.0f;
        const int b0 = jlimit (1, 1023, (int) bin);
        float mag = 0.0f;
        const int b1 = jlimit (b0, 1023, (int) (20.0f * std::pow (1000.0f, (float) (i + 1) / (float) smoothed.size()) / sr * 2048.0f));
        for (int b = b0; b <= b1; ++b) mag = jmax (mag, fftData[(size_t) b]);
        const float db = Decibels::gainToDecibels (mag / 512.0f + 1.0e-9f) + 6.0f;
        smoothed[(size_t) i] = db > smoothed[(size_t) i] ? db : smoothed[(size_t) i] * 0.85f + db * 0.15f;
    }
    repaint();
}

void FilterDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    const float sr = (float) proc.getCurrentSampleRate();

    // grid
    g.setFont (LucidLookAndFeel::font (9.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = hzToX (hz);
        g.setColour (colours::outline.withAlpha (0.5f)); g.drawVerticalLine ((int) x, 8.0f, r.getBottom() - 8.0f);
        g.setColour (colours::textDim.withAlpha (0.7f));
        g.drawText (hz >= 1000.0f ? String (hz / 1000.0f, 0) + "k" : String (hz, 0), (int) x + 2, (int) r.getBottom() - 18, 30, 10, Justification::centredLeft);
    }
    for (float db : { -36.0f, -24.0f, -12.0f, 0.0f, 12.0f })
    {
        const float y = dbToY (db);
        g.setColour (colours::outline.withAlpha (db == 0.0f ? 0.9f : 0.4f)); g.drawHorizontalLine ((int) y, 8.0f, r.getRight() - 8.0f);
    }

    // analyser
    {
        Path spec;
        const float w = r.getWidth() - 16.0f;
        spec.startNewSubPath (8.0f, r.getBottom() - 8.0f);
        for (int i = 0; i < (int) smoothed.size(); ++i)
        {
            const float x = 8.0f + w * (float) i / (float) (smoothed.size() - 1);
            spec.lineTo (x, jlimit (8.0f, r.getBottom() - 8.0f, dbToY (smoothed[(size_t) i])));
        }
        spec.lineTo (r.getRight() - 8.0f, r.getBottom() - 8.0f); spec.closeSubPath();
        g.setGradientFill (ColourGradient (colours::accent.withAlpha (0.22f), 0, r.getY(), colours::accent.withAlpha (0.03f), 0, r.getBottom(), false));
        g.fillPath (spec);
    }

    // response curves
    const Colour cols[kNumFilters] = { colours::filter, colours::filter.withHue (colours::filter.getHue() + 0.12f) };
    for (int i = 0; i < kNumFilters; ++i)
    {
        if (f[i].on->load() < 0.5f) continue;
        const auto type = (FilterType) (int) f[i].type->load();
        const float cutoff = f[i].cutoff->load(), res = f[i].res->load();
        Path curve;
        const int N = 160;
        for (int k = 0; k <= N; ++k)
        {
            const float x = 8.0f + (r.getWidth() - 16.0f) * (float) k / (float) N;
            const float db = Filter::magnitudeDb (type, cutoff, res, xToHz (x), sr);
            const float y = jlimit (2.0f, r.getBottom() - 2.0f, dbToY (db));
            if (k == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        g.setColour (cols[i].withAlpha (0.35f));
        g.strokePath (curve, PathStrokeType (5.0f, PathStrokeType::curved));
        g.setColour (cols[i]);
        g.strokePath (curve, PathStrokeType (1.8f, PathStrokeType::curved));
        const auto hp = handlePos (i);
        g.setColour (cols[i].withAlpha (0.35f)); g.fillEllipse (Rectangle<float> (18, 18).withCentre (hp));
        g.setColour (cols[i]); g.fillEllipse (Rectangle<float> (9, 9).withCentre (hp));
        g.setColour (colours::background); g.drawEllipse (Rectangle<float> (9, 9).withCentre (hp), 1.0f);
        g.setFont (LucidLookAndFeel::font (9.5f, true)); g.setColour (colours::text);
        g.drawText (String (i + 1), Rectangle<float> (14, 14).withCentre ({ hp.x + 12.0f, hp.y - 10.0f }).toNearestInt(), Justification::centred);
    }
}

void FilterDisplay::mouseDown (const MouseEvent& e)
{
    dragging = -1; float best = 22.0f;
    for (int i = 0; i < kNumFilters; ++i)
    {
        if (f[i].on->load() < 0.5f) continue;
        const float d = handlePos (i).getDistanceFrom (e.position);
        if (d < best) { best = d; dragging = i; }
    }
    if (dragging < 0 && f[0].on->load() >= 0.5f) dragging = 0;
    if (dragging >= 0) { f[dragging].cutoffParam->beginChangeGesture(); f[dragging].resParam->beginChangeGesture(); mouseDrag (e); }
}
void FilterDisplay::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const float hz = xToHz (e.position.x);
    const float res = jlimit (0.0f, 1.0f, 1.0f - (e.position.y - 8.0f) / ((float) getHeight() - 16.0f));
    f[dragging].cutoffParam->setValueNotifyingHost (f[dragging].cutoffParam->convertTo0to1 (hz));
    f[dragging].resParam->setValueNotifyingHost (res);
    repaint();
}
void FilterDisplay::mouseUp (const MouseEvent&)
{
    if (dragging >= 0) { f[dragging].cutoffParam->endChangeGesture(); f[dragging].resParam->endChangeGesture(); }
    dragging = -1;
}

// ============================================================================ Envelope
EnvelopeDisplay::EnvelopeDisplay (LucidAudioProcessor& p, int envIndex, Colour accent) : proc (p), env (envIndex), colour (accent)
{
    a = p.apvts.getRawParameterValue (ids::env (env, "a")); d = p.apvts.getRawParameterValue (ids::env (env, "d"));
    s = p.apvts.getRawParameterValue (ids::env (env, "s")); r = p.apvts.getRawParameterValue (ids::env (env, "r"));
    curve = p.apvts.getRawParameterValue (ids::env (env, "curve"));
    setInterceptsMouseClicks (false, false);
}
void EnvelopeDisplay::refresh() { level = proc.envDisplay[env].load(); repaint(); }
void EnvelopeDisplay::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawDisplayBackground (g, bounds);
    const auto area = bounds.reduced (8.0f, 8.0f);
    const float A = a->load(), D = d->load(), S = s->load(), R = r->load(), C = curve->load();
    // Display uses a log-ish time scale so long and short envelopes both read well
    auto seg = [] (float t) { return std::log (1.0f + t * 12.0f); };
    const float sustainW = 0.8f;
    const float total = seg (A) + seg (D) + sustainW + seg (R);
    const float wA = seg (A) / total * area.getWidth(), wD = seg (D) / total * area.getWidth(), wS = sustainW / total * area.getWidth(), wR = seg (R) / total * area.getWidth();
    auto shapeUp = [C] (float t) { return std::pow (t, 1.0f / (1.0f + C * 2.5f)); };          // attack: concave/convex by curve
    auto shapeDown = [C] (float t) { return 1.0f - std::pow (t, 1.0f / (1.0f + C * 2.5f)); };   // decay/release
    Path p;
    const float y0 = area.getBottom(), yTop = area.getY();
    auto Y = [&] (float v) { return y0 - v * (y0 - yTop); };
    p.startNewSubPath (area.getX(), y0);
    float x = area.getX();
    const int N = 24;
    for (int i = 1; i <= N; ++i) p.lineTo (x + wA * (float) i / N, Y (shapeUp ((float) i / N)));
    x += wA;
    for (int i = 1; i <= N; ++i) p.lineTo (x + wD * (float) i / N, Y (S + (1.0f - S) * shapeDown ((float) i / N)));
    x += wD;
    p.lineTo (x + wS, Y (S));
    x += wS;
    for (int i = 1; i <= N; ++i) p.lineTo (x + wR * (float) i / N, Y (S * shapeDown ((float) i / N)));
    Path fill (p); fill.lineTo (area.getRight(), y0); fill.closeSubPath();
    g.setGradientFill (ColourGradient (colour.withAlpha (0.3f), 0, yTop, colour.withAlpha (0.02f), 0, y0, false));
    g.fillPath (fill);
    g.setColour (colour.withAlpha (0.3f)); g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved));
    g.setColour (colour); g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved));
    // live level marker
    if (level > 0.001f)
    {
        const float ly = Y (jlimit (0.0f, 1.0f, level));
        g.setColour (colours::text.withAlpha (0.8f));
        g.drawHorizontalLine ((int) ly, area.getX(), area.getRight());
        g.setColour (colours::text);
        g.fillEllipse (Rectangle<float> (6, 6).withCentre ({ area.getX() + 3.0f, ly }));
    }
}

// ============================================================================ LFO
LfoDisplay::LfoDisplay (LucidAudioProcessor& p, int lfoIndex, Colour accent) : proc (p), lfo (lfoIndex), colour (accent)
{
    shape = p.apvts.getRawParameterValue (ids::lfo (lfo, "shape"));
    phase = p.apvts.getRawParameterValue (ids::lfo (lfo, "phase"));
    setInterceptsMouseClicks (false, false);
}
void LfoDisplay::refresh() { value = proc.lfoDisplay[lfo].load(); phaseAnim += 0.01f; if (phaseAnim > 1.0f) phaseAnim -= 1.0f; repaint(); }
void LfoDisplay::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawDisplayBackground (g, bounds);
    const auto area = bounds.reduced (8.0f, 8.0f);
    const int sh = (int) shape->load();
    const float ph0 = phase->load();
    juce::Random rng (17 + lfo);
    float hold = rng.nextFloat() * 2 - 1, nextV = rng.nextFloat() * 2 - 1;
    int lastCycle = -1;
    Path p;
    const int N = 200;
    for (int i = 0; i <= N; ++i)
    {
        const float t = (float) i / N * 2.0f + ph0; // two cycles
        const float ph = t - std::floor (t);
        const int cycle = (int) std::floor (t);
        if (cycle != lastCycle) { lastCycle = cycle; hold = nextV; nextV = rng.nextFloat() * 2 - 1; }
        float v;
        switch (sh)
        {
            case 0: v = std::sin (kTwoPi * ph); break;
            case 1: v = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
            case 2: v = 2.0f * ph - 1.0f; break;
            case 3: v = 1.0f - 2.0f * ph; break;
            case 4: v = ph < 0.5f ? 1.0f : -1.0f; break;
            case 5: v = hold; break;
            default: { const float tt = ph * ph * (3.0f - 2.0f * ph); v = hold + (nextV - hold) * tt; break; }
        }
        const float x = area.getX() + area.getWidth() * (float) i / N;
        const float y = area.getCentreY() - v * area.getHeight() * 0.45f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colours::outline); g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    g.setColour (colour.withAlpha (0.3f)); g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved));
    g.setColour (colour); g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved));
    // live value bar on the right edge
    const float vy = area.getCentreY() - jlimit (-1.0f, 1.0f, value) * area.getHeight() * 0.45f;
    g.setColour (colours::text); g.fillEllipse (Rectangle<float> (6, 6).withCentre ({ area.getRight() - 3.0f, vy }));
}

// ============================================================================ Scope + meter
ScopeMeter::ScopeMeter (LucidAudioProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
void ScopeMeter::refresh()
{
    proc.scopeFeed.readLatest (samples.data(), (int) samples.size());
    peakL = proc.peakL.load(); peakR = proc.peakR.load();
    repaint();
}
void ScopeMeter::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto meter = bounds.removeFromRight (26.0f);
    drawDisplayBackground (g, bounds);
    const auto area = bounds.reduced (6.0f, 5.0f);
    Path p;
    for (int i = 0; i < (int) samples.size(); ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) (samples.size() - 1);
        const float y = area.getCentreY() - jlimit (-1.0f, 1.0f, samples[(size_t) i]) * area.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colours::accent.withAlpha (0.9f));
    g.strokePath (p, PathStrokeType (1.2f));
    // meters
    auto drawMeter = [&] (Rectangle<float> r, float peak)
    {
        g.setColour (colours::background); g.fillRoundedRectangle (r, 2.0f);
        const float db = Decibels::gainToDecibels (peak + 1.0e-6f);
        const float n = jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        auto fill = r.withTrimmedTop (r.getHeight() * (1.0f - n));
        g.setGradientFill (ColourGradient (colours::red, 0, r.getY(), colours::envelope, 0, r.getBottom(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    };
    drawMeter (meter.reduced (3.0f, 2.0f).removeFromLeft (8.0f), peakL);
    drawMeter (meter.reduced (3.0f, 2.0f).removeFromRight (8.0f), peakR);
}

} // namespace lucid
