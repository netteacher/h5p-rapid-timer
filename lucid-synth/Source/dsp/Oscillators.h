// LUCID Synth - oscillators: wavetable (mip-mapped) and virtual analog (PolyBLEP), unison, sub, noise
#pragma once
#include "Core.h"
#include "Params.h"
#include "Wavetable.h"

namespace lucid {

// PolyBLEP residual for a discontinuity at phase t (0..1) with increment dt
inline float polyBlep (float t, float dt)
{
    if (t < dt)       { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt){ t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct UnisonVoiceState
{
    float phase = 0.0f;
    float detuneRatio = 1.0f;
    float gainL = 0.5f, gainR = 0.5f;
    float triState = 0.0f;   // integrator for analog triangle
    float driftPhase = 0.0f; // slow drift LFO phase
    float driftSeed = 0.0f;
};

class Oscillator
{
public:
    void prepare (float sampleRate, uint32_t seed, const WavetableBank* wavetableBank)
    {
        sr = sampleRate;
        bank = wavetableBank;
        rng = Random (seed);
        for (auto& u : unison) { u.driftSeed = rng.uniform(); u.driftPhase = rng.uniform(); }
        morphSmooth.prepare (sr, 15.0f); morphSmooth.reset (0.0f);
        pwSmooth.prepare (sr, 10.0f);    pwSmooth.reset (0.5f);
    }

    // Called on note-on. resets phases if requested, sets up unison layout.
    void noteOn (const OscParams& p, bool resetPhase)
    {
        configureUnison (p);
        if (resetPhase || p.retrigger)
        {
            for (int i = 0; i < kMaxUnison; ++i)
                unison[(size_t) i].phase = (i == 0 || numUnison == 1) ? p.phase : rng.uniform();
            // random unison phases avoid the "all voices in phase" comb effect at note start
        }
        for (auto& u : unison) u.triState = 0.0f;
        morphSmooth.reset (p.morph);
        pwSmooth.reset (p.pulseWidth);
    }

    void configureUnison (const OscParams& p)
    {
        numUnison = std::max (1, std::min (kMaxUnison, p.unison));
        const float detuneCents = p.detune * p.detune * 100.0f; // 0..100 cents, quadratic taper for fine control
        for (int i = 0; i < numUnison; ++i)
        {
            auto& u = unison[(size_t) i];
            float pos = numUnison > 1 ? ((float) i / (float) (numUnison - 1)) * 2.0f - 1.0f : 0.0f; // -1..1
            // Slightly non-linear spacing gives a richer, less "chorused" stack
            const float shaped = pos * (0.7f + 0.3f * std::fabs (pos));
            u.detuneRatio = centsToRatio (shaped * detuneCents);
            const float pan = shaped * p.spread;
            panGains (pan, u.gainL, u.gainR);
            const float isCentre = (numUnison % 2 == 1 && i == numUnison / 2) || numUnison == 1 ? 1.0f : 0.0f;
            const float g = lerp (p.blend, 1.0f, isCentre);
            u.gainL *= g; u.gainR *= g;
        }
        // Normalise the stack so level doesn't explode with more voices
        float sum = 0.0f;
        for (int i = 0; i < numUnison; ++i) sum += unison[(size_t) i].gainL + unison[(size_t) i].gainR;
        unisonNorm = sum > 0.0f ? (1.0f / std::sqrt (sum)) * 0.85f : 1.0f;
        if (numUnison == 1) unisonNorm = 1.0f;
    }

    // Renders one sample (stereo). baseHz: fundamental after pitch mod, fmIn: phase modulation input (-1..1)
    inline void process (const OscParams& p, const Wavetable* table, float baseHz, float morphMod, float pwMod,
                         float fmIn, float fmAmt, float& outL, float& outR, float& outMono)
    {
        const float morph = clampf (morphSmooth.process() + morphMod, 0.0f, 1.0f);
        const float pw = clampf (pwSmooth.process() + pwMod, 0.03f, 0.97f);
        const float driftAmt = p.drift * p.drift * 0.012f; // up to ~1.2% => ~20 cents
        float l = 0.0f, r = 0.0f, mono = 0.0f;

        for (int i = 0; i < numUnison; ++i)
        {
            auto& u = unison[(size_t) i];
            // Analog drift: slow, per-voice pseudo random wander
            float driftRatio = 1.0f;
            if (driftAmt > 0.0f)
            {
                u.driftPhase += (0.2f + u.driftSeed * 0.5f) / sr;
                if (u.driftPhase >= 1.0f) u.driftPhase -= 1.0f;
                const float d = std::sin (kTwoPi * u.driftPhase) * 0.6f + std::sin (kTwoPi * u.driftPhase * 3.1f + u.driftSeed * 6.0f) * 0.4f;
                driftRatio = 1.0f + d * driftAmt;
            }
            const float hz = baseHz * u.detuneRatio * driftRatio;
            const float dt = clampf (hz / sr, 0.0f, 0.5f);

            float s;
            float ph = u.phase + fmIn * fmAmt * 0.5f;
            ph -= std::floor (ph);
            if (p.engine == OscEngine::Wavetable && table != nullptr)
            {
                s = table->read (ph, morph, Wavetable::mipPosition (hz));
            }
            else
            {
                // Virtual analogue engine: alias-free mip-mapped saw/triangle tables; PWM square is
                // built from two band-limited saws (exact, no DC), sine is computed directly.
                const float mip = Wavetable::mipPosition (hz);
                switch (p.shape)
                {
                    case AnalogShape::Saw:
                        s = bank != nullptr ? bank->analogSaw().read (ph, 0.0f, mip) : (2.0f * ph - 1.0f - polyBlep (ph, dt));
                        break;
                    case AnalogShape::Square:
                    {
                        if (bank != nullptr)
                        {
                            float ph2 = ph + pw; if (ph2 >= 1.0f) ph2 -= 1.0f;
                            const auto& saw = bank->analogSaw();
                            s = saw.read (ph, 0.0f, mip) - saw.read (ph2, 0.0f, mip);
                        }
                        else
                        {
                            s = ph < pw ? 1.0f : -1.0f;
                            s += polyBlep (ph, dt);
                            float t2 = ph - pw; if (t2 < 0.0f) t2 += 1.0f;
                            s -= polyBlep (t2, dt);
                        }
                        break;
                    }
                    case AnalogShape::Triangle:
                        s = bank != nullptr ? bank->analogTriangle().read (ph, 0.0f, mip) : (1.0f - 4.0f * std::fabs (ph - 0.5f));
                        break;
                    default:
                        s = std::sin (kTwoPi * ph);
                        break;
                }
            }
            u.phase += dt;
            if (u.phase >= 1.0f) u.phase -= 1.0f;

            l += s * u.gainL;
            r += s * u.gainR;
            mono += s;
        }
        outL = l * unisonNorm;
        outR = r * unisonNorm;
        outMono = mono / (float) numUnison;
    }

    void setTargets (const OscParams& p)
    {
        morphSmooth.setTarget (p.morph);
        pwSmooth.setTarget (p.pulseWidth);
    }

    int getNumUnison() const { return numUnison; }

private:
    float sr = 48000.0f;
    const WavetableBank* bank = nullptr;
    Random rng;
    std::array<UnisonVoiceState, kMaxUnison> unison;
    int numUnison = 1;
    float unisonNorm = 1.0f;
    Smoother morphSmooth, pwSmooth;
};

// Sub oscillator (sine / triangle / square, one or two octaves down)
class SubOscillator
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }
    void noteOn() { phase = 0.0f; tri = 0.0f; }
    inline float process (SubShape shape, float hz)
    {
        const float dt = clampf (hz / sr, 0.0f, 0.5f);
        float s;
        switch (shape)
        {
            case SubShape::Square:
            {
                s = phase < 0.5f ? 1.0f : -1.0f;
                s += polyBlep (phase, dt);
                float t2 = phase - 0.5f; if (t2 < 0.0f) t2 += 1.0f;
                s -= polyBlep (t2, dt);
                break;
            }
            case SubShape::Triangle:
            {
                float sq = phase < 0.5f ? 1.0f : -1.0f;
                sq += polyBlep (phase, dt);
                float t2 = phase - 0.5f; if (t2 < 0.0f) t2 += 1.0f;
                sq -= polyBlep (t2, dt);
                tri = tri * 0.99995f + 4.0f * dt * sq;
                s = tri;
                break;
            }
            default: s = std::sin (kTwoPi * phase); break;
        }
        phase += dt; if (phase >= 1.0f) phase -= 1.0f;
        return s;
    }
private:
    float sr = 48000.0f, phase = 0.0f, tri = 0.0f;
};

// Stereo noise with a "colour" control: -1 = dark (low-passed), 0 = white, +1 = bright (high-passed)
class NoiseGenerator
{
public:
    void prepare (float sampleRate, uint32_t seed)
    {
        sr = sampleRate; rngL = Random (seed); rngR = Random (seed * 7919u + 13u);
        lpL.setCutoff (1000.0f, sr); lpR.setCutoff (1000.0f, sr);
        hpL.setCutoff (1000.0f, sr); hpR.setCutoff (1000.0f, sr);
    }
    inline void process (float colour, float& l, float& r)
    {
        const float wl = rngL.bipolar(), wr = rngR.bipolar();
        if (colour < 0.0f)
        {
            const float t = -colour;
            l = lerp (wl, lpL.lowpass (wl) * 2.5f, t);
            r = lerp (wr, lpR.lowpass (wr) * 2.5f, t);
        }
        else
        {
            l = lerp (wl, hpL.highpass (wl) * 1.4f, colour);
            r = lerp (wr, hpR.highpass (wr) * 1.4f, colour);
        }
    }
private:
    float sr = 48000.0f;
    Random rngL, rngR;
    OnePole lpL, lpR, hpL, hpR;
};

} // namespace lucid
