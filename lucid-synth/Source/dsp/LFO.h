// LUCID Synth - LFO with tempo sync, fade-in, shapes incl. sample & hold and smooth random
#pragma once
#include "Core.h"
#include "Params.h"

namespace lucid {

class LFO
{
public:
    void prepare (float sampleRate, uint32_t seed)
    {
        sr = sampleRate; rng = Random (seed);
        smooth.prepare (sr, 5.0f);
        holdValue = rng.bipolar(); nextValue = rng.bipolar();
    }
    void retrigger (const LfoParams& p)
    {
        phase = p.phase;
        fadeSamples = p.fadeIn * sr; fadePos = 0.0f;
        holdValue = rng.bipolar(); nextValue = rng.bipolar();
        smooth.reset (0.0f);
    }
    // rateMod: multiplier on rate (e.g. 2^(mod))
    inline float process (const LfoParams& p, double bpm, float rateMul)
    {
        float hz;
        if (p.sync)
        {
            const float beats = syncDivisionBeats (p.syncDiv);
            hz = (float) (bpm / 60.0) / beats;
        }
        else hz = p.rateHz;
        hz *= rateMul;
        const float dt = clampf (hz / sr, 0.0f, 0.5f);

        float v;
        switch (p.shape)
        {
            case LfoShape::Sine:     v = std::sin (kTwoPi * phase); break;
            case LfoShape::Triangle: v = 1.0f - 4.0f * std::fabs (phase - 0.5f); break;
            case LfoShape::SawUp:    v = 2.0f * phase - 1.0f; break;
            case LfoShape::SawDown:  v = 1.0f - 2.0f * phase; break;
            case LfoShape::Square:   v = phase < 0.5f ? 1.0f : -1.0f; break;
            case LfoShape::SampleHold: v = holdValue; break;
            case LfoShape::SmoothRandom:
            {
                const float t = phase; const float tt = t * t * (3.0f - 2.0f * t);
                v = lerp (holdValue, nextValue, tt); break;
            }
            default: v = 0.0f; break;
        }
        phase += dt;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            holdValue = nextValue; nextValue = rng.bipolar();
        }
        float fade = 1.0f;
        if (fadeSamples > 1.0f)
        {
            fade = clampf (fadePos / fadeSamples, 0.0f, 1.0f);
            fadePos += 1.0f;
        }
        v *= fade;
        if (p.smooth > 0.001f)
        {
            smooth.coef = std::exp (-1.0f / (0.001f * (1.0f + p.smooth * 200.0f) * sr));
            smooth.setTarget (v);
            v = smooth.process();
        }
        return v;
    }
    float getPhase() const { return phase; }

private:
    float sr = 48000.0f, phase = 0.0f;
    float fadeSamples = 0.0f, fadePos = 0.0f;
    float holdValue = 0.0f, nextValue = 0.0f;
    Random rng;
    Smoother smooth;
};

} // namespace lucid
