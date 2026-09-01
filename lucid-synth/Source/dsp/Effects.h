// LUCID Synth - master effects: saturator (oversampled), 3-band EQ, compressor, chorus,
// stereo/ping-pong delay, FDN reverb, lookahead limiter.
#pragma once
#include "Core.h"
#include "Params.h"
#include "Filters.h"
#include <vector>

namespace lucid {

// Fractional delay line (linear interpolation)
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        size = 1; while (size < maxSamples + 2) size <<= 1;
        mask = size - 1; buf.assign ((size_t) size, 0.0f); w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (float delaySamples) const
    {
        const float d = clampf (delaySamples, 1.0f, (float) (size - 2));
        const int di = (int) d; const float f = d - (float) di;
        const int r0 = (w - di - 1 + size) & mask;   // -1 because write already advanced
        const int r1 = (r0 - 1 + size) & mask;
        return buf[(size_t) r0] + f * (buf[(size_t) r1] - buf[(size_t) r0]);
    }
    inline float readInt (int delaySamples) const
    {
        const int di = std::max (1, std::min (delaySamples, size - 2));
        return buf[(size_t) ((w - di - 1 + size) & mask)];
    }
    int capacity() const { return size - 2; }
private:
    std::vector<float> buf; int size = 0, mask = 0, w = 0;
};

// Schroeder allpass for diffusion
struct Allpass
{
    DelayLine line; float g = 0.7f; int len = 100;
    void prepare (int samples, float gain) { line.prepare (samples + 4); len = samples; g = gain; }
    void reset() { line.reset(); }
    inline float process (float x)
    {
        const float d = line.readInt (len);
        const float v = x - g * d;
        line.write (v);
        return d + g * v;
    }
};

// ---------------------------------------------------------------------------- Saturator
class Saturator
{
public:
    void prepare (float sampleRate) { sr = sampleRate; for (auto& o : os) o.reset(); driveSmooth.prepare (sr, 20.0f); driveSmooth.reset (0.0f); for (auto& d : dc) { d.prepare (sr); d.reset(); } }
    void process (float* l, float* r, int n, const FxParams& p)
    {
        driveSmooth.setTarget (p.satDrive);
        for (int i = 0; i < n; ++i)
        {
            const float drive = driveSmooth.process();
            const float gain = 1.0f + drive * drive * 24.0f;
            const float comp = 1.0f / std::sqrt (gain);
            float* ch[2] = { l, r };
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                float a, b; os[(size_t) c].up (x * gain, a, b);
                a = shape (a, p.satType); b = shape (b, p.satType);
                float y = os[(size_t) c].down (a, b) * comp;
                y = dc[(size_t) c].process (y);
                ch[c][i] = lerp (x, y, p.satMix);
            }
        }
    }
private:
    static inline float shape (float x, int type)
    {
        switch (type)
        {
            case 0: return fastTanh (x);                                       // Tape
            case 1: { const float y = x + 0.28f * x * x - 0.1f; return fastTanh (y) * 1.1f; } // Tube (even harmonics)
            case 2: { float t = x * 0.25f + 0.25f; t -= std::floor (t); return std::fabs (t * 4.0f - 2.0f) - 1.0f; } // Fold
            default: return clampf (x, -1.0f, 1.0f);                           // Hard
        }
    }
    float sr = 48000.0f;
    std::array<HalfBand2x, 2> os;
    std::array<DcBlocker, 2> dc;
    Smoother driveSmooth;
};

// ---------------------------------------------------------------------------- EQ (RBJ biquads)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0.0f; }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    enum Type { LowShelf, Peak, HighShelf };
    void set (Type t, float hz, float gainDb, float q, float sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = kTwoPi * clampf (hz, 10.0f, sr * 0.45f) / sr;
        const float cw = std::cos (w0), sw = std::sin (w0);
        float B0, B1, B2, A0, A1, A2;
        if (t == Peak)
        {
            const float alpha = sw / (2.0f * std::max (q, 0.05f));
            B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A;
            A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A;
        }
        else
        {
            const float alpha = sw / 2.0f * std::sqrt ((A + 1.0f / A) * (1.0f / 0.9f - 1.0f) + 2.0f);
            const float sqA2a = 2.0f * std::sqrt (A) * alpha;
            if (t == LowShelf)
            {
                B0 = A * ((A + 1) - (A - 1) * cw + sqA2a); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sqA2a);
                A0 = (A + 1) + (A - 1) * cw + sqA2a;       A1 = -2 * ((A - 1) + (A + 1) * cw);   A2 = (A + 1) + (A - 1) * cw - sqA2a;
            }
            else
            {
                B0 = A * ((A + 1) + (A - 1) * cw + sqA2a); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sqA2a);
                A0 = (A + 1) - (A - 1) * cw + sqA2a;       A1 = 2 * ((A - 1) - (A + 1) * cw);     A2 = (A + 1) - (A - 1) * cw - sqA2a;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    // magnitude in dB at hz
    float magnitudeDb (float hz, float sr) const
    {
        const float w = kTwoPi * hz / sr;
        const float c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const float dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return gainToDb (std::sqrt ((nr * nr + ni * ni) / std::max (dr * dr + di * di, 1.0e-12f)));
    }
};

class Equalizer
{
public:
    void prepare (float sampleRate) { sr = sampleRate; for (auto& c : bands) for (auto& b : c) b.reset(); lastKey = -1.0f; }
    void update (const FxParams& p)
    {
        const float key = p.eqLowFreq * 1.1f + p.eqLowGain * 3.7f + p.eqMidFreq * 0.13f + p.eqMidGain * 5.1f + p.eqMidQ * 7.3f + p.eqHighFreq * 0.017f + p.eqHighGain * 11.1f;
        if (key == lastKey) return;
        lastKey = key;
        for (auto& c : bands)
        {
            c[0].set (Biquad::LowShelf,  p.eqLowFreq,  p.eqLowGain,  0.7f, sr);
            c[1].set (Biquad::Peak,      p.eqMidFreq,  p.eqMidGain,  p.eqMidQ, sr);
            c[2].set (Biquad::HighShelf, p.eqHighFreq, p.eqHighGain, 0.7f, sr);
        }
    }
    void process (float* l, float* r, int n, const FxParams& p)
    {
        update (p);
        for (int i = 0; i < n; ++i)
        {
            l[i] = bands[0][2].process (bands[0][1].process (bands[0][0].process (l[i])));
            r[i] = bands[1][2].process (bands[1][1].process (bands[1][0].process (r[i])));
        }
    }
    float magnitudeDb (float hz) const { return bands[0][0].magnitudeDb (hz, sr) + bands[0][1].magnitudeDb (hz, sr) + bands[0][2].magnitudeDb (hz, sr); }
private:
    float sr = 48000.0f, lastKey = -1.0f;
    std::array<std::array<Biquad, 3>, 2> bands;
};

// ---------------------------------------------------------------------------- Compressor
class Compressor
{
public:
    void prepare (float sampleRate) { sr = sampleRate; grSmoothed = 0.0f; }
    void process (float* l, float* r, int n, const FxParams& p)
    {
        const float att = std::exp (-1.0f / (std::max (0.0001f, p.compAttack) * sr));
        const float rel = std::exp (-1.0f / (std::max (0.001f, p.compRelease) * sr));
        const float knee = 6.0f, slope = 1.0f - 1.0f / std::max (1.0f, p.compRatio);
        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max (std::fabs (l[i]), std::fabs (r[i]));
            const float over = gainToDb (peak + 1.0e-9f) - p.compThreshold;
            float gr;
            if (over <= -knee * 0.5f) gr = 0.0f;
            else if (over >= knee * 0.5f) gr = over * slope;
            else { const float t = over + knee * 0.5f; gr = slope * t * t / (2.0f * knee); }
            grSmoothed = gr > grSmoothed ? gr + (grSmoothed - gr) * att : gr + (grSmoothed - gr) * rel;
            const float g = dbToGain (-grSmoothed + p.compMakeup);
            l[i] = lerp (l[i], l[i] * g, p.compMix);
            r[i] = lerp (r[i], r[i] * g, p.compMix);
        }
        currentGr = grSmoothed;
    }
    float getGainReductionDb() const { return currentGr; }
private:
    float sr = 48000.0f, grSmoothed = 0.0f, currentGr = 0.0f;
};

// ---------------------------------------------------------------------------- Chorus
class Chorus
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines) { d.prepare ((int) (sr * 0.05f) + 8); d.reset(); }
        phase = 0.0f; mixSmooth.prepare (sr, 20.0f); mixSmooth.reset (0.0f);
    }
    void process (float* l, float* r, int n, const FxParams& p, float mixMod)
    {
        mixSmooth.setTarget (clampf (p.chorusMix + mixMod, 0.0f, 1.0f));
        const float dt = p.chorusRate / sr;
        const float base = 0.007f * sr, depth = p.chorusDepth * 0.005f * sr;
        for (int i = 0; i < n; ++i)
        {
            const float mix = mixSmooth.process();
            lines[0].write (l[i]); lines[1].write (r[i]);
            float wl = 0.0f, wr = 0.0f;
            // three taps per channel, phase-staggered; right channel offset by 'width'
            for (int t = 0; t < 3; ++t)
            {
                const float phL = phase + (float) t / 3.0f;
                const float phR = phL + 0.5f * p.chorusWidth / 3.0f + 0.25f * p.chorusWidth;
                wl += lines[0].read (base + depth * (0.5f + 0.5f * std::sin (kTwoPi * phL)) + (float) t * 0.0011f * sr);
                wr += lines[1].read (base + depth * (0.5f + 0.5f * std::sin (kTwoPi * phR)) + (float) t * 0.0011f * sr);
            }
            wl *= 0.4f; wr *= 0.4f;
            l[i] = l[i] * (1.0f - mix * 0.5f) + wl * mix;
            r[i] = r[i] * (1.0f - mix * 0.5f) + wr * mix;
            phase += dt; if (phase >= 1.0f) phase -= 1.0f;
        }
    }
private:
    float sr = 48000.0f, phase = 0.0f;
    std::array<DelayLine, 2> lines;
    Smoother mixSmooth;
};

// ---------------------------------------------------------------------------- Delay
class StereoDelay
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines) { d.prepare ((int) (sr * 4.0f) + 16); d.reset(); }
        timeSmooth.prepare (sr, 80.0f); timeSmooth.reset (0.25f * sr);
        mixSmooth.prepare (sr, 20.0f); mixSmooth.reset (0.0f);
        fbSmooth.prepare (sr, 20.0f); fbSmooth.reset (0.0f);
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
    }
    void process (float* l, float* r, int n, const FxParams& p, double bpm, float mixMod, float fbMod)
    {
        float timeSec = p.delaySync ? (float) (60.0 / bpm) * syncDivisionBeats (p.delayDiv) : p.delayTimeMs * 0.001f;
        timeSec = clampf (timeSec, 0.001f, 3.9f);
        timeSmooth.setTarget (timeSec * sr);
        mixSmooth.setTarget (clampf (p.delayMix + mixMod, 0.0f, 1.0f));
        fbSmooth.setTarget (clampf (p.delayFeedback + fbMod, 0.0f, 1.1f));
        for (int c = 0; c < 2; ++c) { lp[(size_t) c].setCutoff (p.delayHighCut, sr); hp[(size_t) c].setCutoff (p.delayLowCut, sr); }
        for (int i = 0; i < n; ++i)
        {
            const float d = timeSmooth.process();
            const float mix = mixSmooth.process();
            const float fb = fbSmooth.process();
            const float dl = lines[0].read (d), dr = lines[1].read (d);
            float fl = fastTanh (hp[0].highpass (lp[0].lowpass (dl)) * fb);
            float fr = fastTanh (hp[1].highpass (lp[1].lowpass (dr)) * fb);
            if (p.delayPingPong)
            {
                const float in = (l[i] + r[i]) * 0.5f;
                lines[0].write (in + fr);
                lines[1].write (fl);
            }
            else
            {
                lines[0].write (l[i] + fl);
                lines[1].write (r[i] + fr);
            }
            l[i] += dl * mix;
            r[i] += dr * mix;
        }
    }
private:
    float sr = 48000.0f;
    std::array<DelayLine, 2> lines;
    std::array<OnePole, 2> lp, hp;
    Smoother timeSmooth, mixSmooth, fbSmooth;
};

// ---------------------------------------------------------------------------- Reverb (8-line FDN)
class Reverb
{
public:
    static constexpr int N = 8;
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        preDelay.prepare ((int) (sr * 0.3f) + 8);
        static const float apMs[4] = { 4.77f, 3.6f, 12.73f, 9.3f };
        for (int i = 0; i < 4; ++i) { apL[i].prepare ((int) (apMs[i] * 0.001f * sr), 0.72f); apR[i].prepare ((int) (apMs[i] * 0.001f * sr * 1.07f), 0.72f); }
        for (int i = 0; i < N; ++i) { lines[i].prepare ((int) (sr * 0.25f) + 8); damp[i].reset(); fb[i] = 0.0f; modPhase[i] = (float) i / (float) N; }
        sizeSmooth.prepare (sr, 200.0f); sizeSmooth.reset (0.6f);
        mixSmooth.prepare (sr, 30.0f); mixSmooth.reset (0.0f);
        reset();
    }
    void reset()
    {
        preDelay.reset(); for (auto& a : apL) a.reset(); for (auto& a : apR) a.reset();
        for (int i = 0; i < N; ++i) { lines[i].reset(); damp[i].reset(); fb[i] = 0.0f; }
    }
    void process (float* l, float* r, int n, const FxParams& p, float mixMod, float sizeMod)
    {
        static const float baseMs[N] = { 37.1f, 43.7f, 51.3f, 59.9f, 67.3f, 79.1f, 89.7f, 101.3f };
        sizeSmooth.setTarget (clampf (p.reverbSize + sizeMod, 0.0f, 1.0f));
        mixSmooth.setTarget (clampf (p.reverbMix + mixMod, 0.0f, 1.0f));
        const float t60 = 0.2f + p.reverbDecay * p.reverbDecay * 14.0f;
        const float dampHz = lerp (18000.0f, 1200.0f, p.reverbDamping * p.reverbDamping);
        for (int i = 0; i < N; ++i) damp[i].setCutoff (dampHz, sr);
        const float pre = clampf (p.reverbPreDelay, 0.0f, 250.0f) * 0.001f * sr;
        const float modDepth = 0.35f * 0.001f * sr;

        for (int s = 0; s < n; ++s)
        {
            const float size = sizeSmooth.process();
            const float sizeMul = 0.35f + size * 1.65f;
            const float mix = mixSmooth.process();
            const float inMono = (l[s] + r[s]) * 0.5f;
            preDelay.write (inMono);
            float x = pre > 1.0f ? preDelay.read (pre) : inMono;
            float xl = x, xr = x;
            for (int i = 0; i < 4; ++i) { xl = apL[i].process (xl); xr = apR[i].process (xr); }

            // read the delay lines (modulated), apply damping + decay
            float out[N];
            float sum = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                modPhase[i] += (0.11f + 0.037f * (float) i) / sr; if (modPhase[i] >= 1.0f) modPhase[i] -= 1.0f;
                const float len = baseMs[i] * 0.001f * sr * sizeMul + modDepth * std::sin (kTwoPi * modPhase[i]);
                float v = lines[i].read (len);
                v = damp[i].lowpass (v);
                const float g = std::pow (10.0f, -3.0f * (len / sr) / t60);
                out[i] = v * g;
                sum += out[i];
            }
            // Householder feedback matrix: y = x - (2/N) * sum(x)
            const float h = sum * (2.0f / (float) N);
            for (int i = 0; i < N; ++i)
            {
                const float inj = (i & 1) ? xr : xl;
                lines[i].write (sanitize (out[i] - h + inj * 0.5f));
            }
            float wl = (out[0] - out[2] + out[4] - out[6]) * 0.5f;
            float wr = (out[1] - out[3] + out[5] - out[7]) * 0.5f;
            const float mid = (wl + wr) * 0.5f, side = (wl - wr) * 0.5f * p.reverbWidth;
            wl = mid + side; wr = mid - side;
            l[s] = l[s] * (1.0f - mix * 0.6f) + wl * mix;
            r[s] = r[s] * (1.0f - mix * 0.6f) + wr * mix;
        }
    }
private:
    float sr = 48000.0f;
    DelayLine preDelay;
    Allpass apL[4], apR[4];
    DelayLine lines[N];
    OnePole damp[N];
    float fb[N] = {};
    float modPhase[N] = {};
    Smoother sizeSmooth, mixSmooth;
};

// ---------------------------------------------------------------------------- Lookahead limiter
class Limiter
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        look = std::max (8, (int) (sr * 0.002f));
        for (auto& d : delay) d.assign ((size_t) look, 0.0f);
        needed.assign ((size_t) look, 1.0f);
        minWin.assign ((size_t) look, 1.0f);
        pos = 0; runningSum = (float) look; gain = 1.0f;
        relCoef = std::exp (-1.0f / (0.08f * sr));
    }
    void process (float* l, float* r, int n, float ceilingDb)
    {
        const float ceil = dbToGain (ceilingDb);
        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max (std::fabs (l[i]), std::fabs (r[i]));
            const float need = peak > ceil ? ceil / peak : 1.0f;
            // ring buffers
            const float outL = delay[0][(size_t) pos], outR = delay[1][(size_t) pos];
            delay[0][(size_t) pos] = l[i]; delay[1][(size_t) pos] = r[i];
            needed[(size_t) pos] = need;
            // moving minimum over the lookahead window (brute force, window is tiny)
            float m = 1.0f; for (int k = 0; k < look; ++k) m = std::min (m, needed[(size_t) k]);
            // moving average of the minimum for a smooth attack shape
            runningSum += m - minWin[(size_t) pos]; minWin[(size_t) pos] = m;
            const float avg = runningSum / (float) look;
            float target = std::min (m, avg);
            if (target < gain) gain = target; else gain = target + (gain - target) * relCoef;
            pos = (pos + 1) % look;
            l[i] = clampf (outL * gain, -ceil, ceil);
            r[i] = clampf (outR * gain, -ceil, ceil);
        }
        currentGain = gain;
    }
    float getGainReductionDb() const { return -gainToDb (currentGain); }
    int getLatency() const { return look; }
private:
    float sr = 48000.0f; int look = 96, pos = 0; float gain = 1.0f, relCoef = 0.999f, runningSum = 0.0f, currentGain = 1.0f;
    std::array<std::vector<float>, 2> delay;
    std::vector<float> needed, minWin;
};

// ---------------------------------------------------------------------------- Chain
struct FxModulation { float chorusMix = 0, delayMix = 0, delayFeedback = 0, reverbMix = 0, reverbSize = 0; };

class EffectsChain
{
public:
    void prepare (float sampleRate)
    {
        sat.prepare (sampleRate); eq.prepare (sampleRate); comp.prepare (sampleRate);
        chorus.prepare (sampleRate); delay.prepare (sampleRate); reverb.prepare (sampleRate); limiter.prepare (sampleRate);
        gainSmooth.prepare (sampleRate, 20.0f); gainSmooth.reset (1.0f);
    }
    void process (float* l, float* r, int n, const SynthParams& p, const FxModulation& m)
    {
        const auto& fx = p.fx;
        if (fx.satOn)    sat.process (l, r, n, fx);
        if (fx.eqOn)     eq.process (l, r, n, fx);
        if (fx.compOn)   comp.process (l, r, n, fx);
        if (fx.chorusOn) chorus.process (l, r, n, fx, m.chorusMix);
        if (fx.delayOn)  delay.process (l, r, n, fx, p.bpm, m.delayMix, m.delayFeedback);
        if (fx.reverbOn) reverb.process (l, r, n, fx, m.reverbMix, m.reverbSize);
        gainSmooth.setTarget (dbToGain (p.masterGain));
        for (int i = 0; i < n; ++i) { const float g = gainSmooth.process(); l[i] *= g; r[i] *= g; }
        if (fx.limiterOn) limiter.process (l, r, n, fx.limiterCeiling);
        else for (int i = 0; i < n; ++i) { l[i] = softClip (l[i]); r[i] = softClip (r[i]); }
        for (int i = 0; i < n; ++i) { l[i] = sanitize (l[i]); r[i] = sanitize (r[i]); }
    }
    int getLatency (const SynthParams& p) const { return p.fx.limiterOn ? limiter.getLatency() : 0; }
    const Equalizer& getEq() const { return eq; }
    const Compressor& getCompressor() const { return comp; }
    const Limiter& getLimiter() const { return limiter; }
private:
    Saturator sat; Equalizer eq; Compressor comp; Chorus chorus; StereoDelay delay; Reverb reverb; Limiter limiter;
    Smoother gainSmooth;
};

} // namespace lucid
