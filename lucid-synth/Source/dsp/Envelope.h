// LUCID Synth - ADSR envelope with analogue-style exponential segments and a curve control
#pragma once
#include "Core.h"
#include "Params.h"

namespace lucid {

class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare (float sampleRate) { sr = sampleRate; }

    void noteOn (const EnvParams& p, float velocity01)
    {
        params = p;
        velScale = lerp (1.0f, velocity01, p.velocity);
        stage = Stage::Attack;
        computeCoefs();
        // retrigger from the current level (no click)
    }
    void noteOff()
    {
        if (stage != Stage::Idle) { stage = Stage::Release; computeCoefs(); }
    }
    void kill() { stage = Stage::Idle; level = 0.0f; }
    void setParams (const EnvParams& p) { params = p; computeCoefs(); }

    inline float process()
    {
        switch (stage)
        {
            case Stage::Attack:
                level = attackBase + level * attackCoef;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                level = decayBase + level * decayCoef;
                if (level <= params.sustain + 1.0e-4f) { level = params.sustain; stage = Stage::Sustain; }
                break;
            case Stage::Sustain:
                level = params.sustain;
                break;
            case Stage::Release:
                level = releaseBase + level * releaseCoef;
                if (level <= 1.0e-4f) { level = 0.0f; stage = Stage::Idle; }
                break;
            default: level = 0.0f; break;
        }
        return level * velScale;
    }

    bool isActive() const { return stage != Stage::Idle; }
    bool isReleasing() const { return stage == Stage::Release; }
    float getLevel() const { return level * velScale; }
    Stage getStage() const { return stage; }

private:
    float sr = 48000.0f;
    EnvParams params;
    Stage stage = Stage::Idle;
    float level = 0.0f, velScale = 1.0f;
    float attackCoef = 0, attackBase = 0, decayCoef = 0, decayBase = 0, releaseCoef = 0, releaseBase = 0;

    // "target ratio" technique: exponential segments that reach their target in the given time.
    static float calcCoef (float rate, float targetRatio)
    {
        return rate <= 0.0f ? 0.0f : std::exp (-std::log ((1.0f + targetRatio) / targetRatio) / rate);
    }
    void computeCoefs()
    {
        // curve 0 -> nearly linear (large ratio), 1 -> steep exponential (tiny ratio)
        const float c = clampf (params.curve, 0.0f, 1.0f);
        const float attackRatio  = lerp (3.0f, 0.03f, c);
        const float decayRatio   = lerp (3.0f, 0.0005f, c);
        const float minTime = 0.0005f;
        const float aRate = std::max (minTime, params.attack) * sr;
        const float dRate = std::max (minTime, params.decay) * sr;
        const float rRate = std::max (minTime, params.release) * sr;
        attackCoef = calcCoef (aRate, attackRatio);
        attackBase = (1.0f + attackRatio) * (1.0f - attackCoef);
        decayCoef = calcCoef (dRate, decayRatio);
        decayBase = (params.sustain - decayRatio) * (1.0f - decayCoef);
        releaseCoef = calcCoef (rRate, decayRatio);
        releaseBase = -decayRatio * (1.0f - releaseCoef);
    }
};

} // namespace lucid
