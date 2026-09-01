// LUCID Synth - a single polyphonic voice: 2 oscillators + sub + noise, 2 stereo filters,
// 3 envelopes, 3 LFOs and the per-voice modulation matrix.
#pragma once
#include "Core.h"
#include "Params.h"
#include "Oscillators.h"
#include "Filters.h"
#include "Envelope.h"
#include "LFO.h"

namespace lucid {

// Values shared by all voices for the current block (set by the engine)
struct GlobalModState
{
    float modWheel = 0.0f, aftertouch = 0.0f, pitchBend = 0.0f; // bend -1..1
    std::array<float, kNumMacros> macros { 0, 0, 0, 0 };
    const float* globalLfo[kNumLfos] = { nullptr, nullptr, nullptr };   // per-sample buffers for mono LFOs
};

class Voice
{
public:
    static constexpr int kControlInterval = 16;

    void prepare (float sampleRate, uint32_t seed, const WavetableBank* wavetables)
    {
        sr = sampleRate; bank = wavetables; rng = Random (seed);
        for (int i = 0; i < kNumOscs; ++i) osc[(size_t) i].prepare (sr, seed * 3u + (uint32_t) i * 101u, bank);
        sub.prepare (sr);
        noise.prepare (sr, seed * 17u + 5u);
        for (auto& fc : filters) for (auto& f : fc) f.prepare (sr);
        for (auto& e : envs) e.prepare (sr);
        for (int i = 0; i < kNumLfos; ++i) lfos[(size_t) i].prepare (sr, seed * 7u + (uint32_t) i * 13u);
        glideCoef = 0.0f;
        modCur.fill (0.0f); modStep.fill (0.0f);
    }

    void noteOn (int midiNote, float velocity01, const SynthParams& p, float glideFromNote, bool legato, uint64_t stamp)
    {
        note = midiNote; velocity = velocity01; startStamp = stamp; releasedFlag = false;
        randomValue = rng.bipolar();
        keyTrackValue = ((float) midiNote - 60.0f) / 60.0f;
        targetNote = (float) midiNote;
        if (! legato || ! active)
        {
            currentNote = (p.glideTime > 0.0f && glideFromNote > 0.0f) ? glideFromNote : (float) midiNote;
            for (int i = 0; i < kNumOscs; ++i) osc[(size_t) i].noteOn (p.osc[(size_t) i], ! active);
            sub.noteOn();
            for (int i = 0; i < kNumEnvs; ++i) envs[(size_t) i].noteOn (p.env[(size_t) i], velocity01);
            for (int i = 0; i < kNumLfos; ++i) lfos[(size_t) i].retrigger (p.lfo[(size_t) i]);
            if (! active)
            {
                for (auto& fc : filters) for (auto& f : fc) f.reset();
                for (int f = 0; f < kNumFilters; ++f)
                    for (int c = 0; c < 2; ++c)
                        filters[(size_t) f][(size_t) c].resetTo (p.filter[(size_t) f].cutoff, p.filter[(size_t) f].resonance, p.filter[(size_t) f].drive);
                evaluateModulation (p, GlobalModState(), 0);
                for (int d = 0; d < (int) ModDest::NumDests; ++d) modCur[(size_t) d] = modNext[(size_t) d];
                modStep.fill (0.0f);
            }
        }
        glideCoef = p.glideTime > 0.0005f ? std::exp (-1.0f / (p.glideTime * sr)) : 0.0f;
        active = true;
        controlCounter = 0;
    }

    void noteOff()
    {
        releasedFlag = true;
        for (auto& e : envs) e.noteOff();
    }
    void kill() { active = false; for (auto& e : envs) e.kill(); }

    bool isActive() const { return active; }
    bool isReleasing() const { return releasedFlag; }
    int getNote() const { return note; }
    uint64_t getStamp() const { return startStamp; }
    float getAmpLevel() const { return envs[0].getLevel(); }
    float getEnvLevel (int i) const { return envs[(size_t) i].getLevel(); }
    float getLastLfo (int i) const { return lastLfo[(size_t) i]; }
    float getVelocity() const { return velocity; }
    float getKeyTrack() const { return keyTrackValue; }
    float getRandom() const { return randomValue; }

    // Renders and ADDS into outL/outR
    void render (const SynthParams& p, const GlobalModState& gm, float* outL, float* outR, int numSamples)
    {
        if (! active) return;
        const auto& pa = p.osc[0];
        const auto& pb = p.osc[1];
        const Wavetable* tabA = bank != nullptr ? &bank->get (pa.table) : nullptr;
        const Wavetable* tabB = bank != nullptr ? &bank->get (pb.table) : nullptr;
        for (int i = 0; i < kNumOscs; ++i) osc[(size_t) i].setTargets (p.osc[(size_t) i]);
        for (int i = 0; i < kNumEnvs; ++i) envs[(size_t) i].setParams (p.env[(size_t) i]);

        const float bendSemis = gm.pitchBend * p.pitchBendRange;
        const float offsetA = (float) (pa.octave * 12 + pa.semi) + pa.fine * 0.01f;
        const float offsetB = (float) (pb.octave * 12 + pb.semi) + pb.fine * 0.01f;
        const float subRatio = p.subOctave >= 2 ? 0.25f : 0.5f;

        for (int s = 0; s < numSamples; ++s)
        {
            // control-rate modulation update
            if (controlCounter == 0)
            {
                evaluateModulation (p, gm, s);
                for (int d = 0; d < (int) ModDest::NumDests; ++d)
                    modStep[(size_t) d] = (modNext[(size_t) d] - modCur[(size_t) d]) / (float) kControlInterval;
                updateFilterTargets (p);
            }
            controlCounter = (controlCounter + 1) % kControlInterval;
            for (int d = 0; d < (int) ModDest::NumDests; ++d) modCur[(size_t) d] += modStep[(size_t) d];

            // envelopes & LFOs (per sample)
            envLevel[0] = envs[0].process(); envLevel[1] = envs[1].process(); envLevel[2] = envs[2].process();
            for (int i = 0; i < kNumLfos; ++i)
            {
                const auto& lp = p.lfo[(size_t) i];
                if (lp.mono && gm.globalLfo[i] != nullptr) lastLfo[(size_t) i] = gm.globalLfo[i][s];
                else
                {
                    const float rateMul = std::exp2 (modCur[(size_t) (int) ModDest::Lfo1Rate + i] * 4.0f);
                    lastLfo[(size_t) i] = lfos[(size_t) i].process (lp, p.bpm, rateMul);
                }
            }

            // glide
            if (glideCoef > 0.0f) currentNote = targetNote + (currentNote - targetNote) * glideCoef;
            else currentNote = targetNote;

            const float baseNote = currentNote + bendSemis + modCur[(size_t) ModDest::PitchAll] * 24.0f;
            const float hzA = midiToHz (baseNote + offsetA + modCur[(size_t) ModDest::OscAPitch] * 24.0f);
            const float hzB = midiToHz (baseNote + offsetB + modCur[(size_t) ModDest::OscBPitch] * 24.0f);

            float aL = 0, aR = 0, aM = 0, bL = 0, bR = 0, bM = 0;
            if (pb.enabled)
                osc[1].process (pb, tabB, hzB, modCur[(size_t) ModDest::OscBMorph], modCur[(size_t) ModDest::OscBPW] * 0.45f, 0.0f, 0.0f, bL, bR, bM);
            if (pa.enabled)
            {
                const float fm = clampf (p.fmAmount + modCur[(size_t) ModDest::FmAmount], 0.0f, 1.0f);
                osc[0].process (pa, tabA, hzA, modCur[(size_t) ModDest::OscAMorph], modCur[(size_t) ModDest::OscAPW] * 0.45f, bM, fm * fm * 2.0f, aL, aR, aM);
                if (p.ringMod > 0.0f)
                {
                    aL = lerp (aL, aL * bM, p.ringMod);
                    aR = lerp (aR, aR * bM, p.ringMod);
                }
            }
            const float levelA = clampf (pa.level + modCur[(size_t) ModDest::OscALevel], 0.0f, 1.0f);
            const float levelB = clampf (pb.level + modCur[(size_t) ModDest::OscBLevel], 0.0f, 1.0f);
            float gal, gar, gbl, gbr;
            panGains (pa.pan + modCur[(size_t) ModDest::OscAPan], gal, gar);
            panGains (pb.pan + modCur[(size_t) ModDest::OscBPan], gbl, gbr);
            aL *= levelA * gal * 1.4142f; aR *= levelA * gar * 1.4142f;
            bL *= levelB * gbl * 1.4142f; bR *= levelB * gbr * 1.4142f;

            const float subLevel = clampf (p.subLevel + modCur[(size_t) ModDest::SubLevel], 0.0f, 1.0f);
            float subS = 0.0f;
            if (subLevel > 0.0f) subS = sub.process (p.subShape, midiToHz (baseNote + offsetA) * subRatio) * subLevel;

            const float noiseLevel = clampf (p.noiseLevel + modCur[(size_t) ModDest::NoiseLevel], 0.0f, 1.0f);
            float nL = 0.0f, nR = 0.0f;
            if (noiseLevel > 0.0f) { noise.process (p.noiseColor, nL, nR); nL *= noiseLevel * 0.5f; nR *= noiseLevel * 0.5f; }

            // filter routing
            float yL, yR;
            const bool f1 = p.filter[0].enabled, f2 = p.filter[1].enabled;
            const FilterType t1 = p.filter[0].type, t2 = p.filter[1].type;
            switch (p.routing)
            {
                case FilterRouting::Parallel:
                {
                    const float xL = aL + bL + subS + nL, xR = aR + bR + subS + nR;
                    const float m = p.filterMix;
                    const float g1 = std::cos (m * kPi * 0.5f), g2 = std::sin (m * kPi * 0.5f);
                    const float y1L = f1 ? filters[0][0].process (xL, t1) : xL, y1R = f1 ? filters[0][1].process (xR, t1) : xR;
                    const float y2L = f2 ? filters[1][0].process (xL, t2) : xL, y2R = f2 ? filters[1][1].process (xR, t2) : xR;
                    yL = y1L * g1 + y2L * g2; yR = y1R * g1 + y2R * g2;
                    break;
                }
                case FilterRouting::Split:
                {
                    const float x1L = aL + subS, x1R = aR + subS, x2L = bL + nL, x2R = bR + nR;
                    yL = (f1 ? filters[0][0].process (x1L, t1) : x1L) + (f2 ? filters[1][0].process (x2L, t2) : x2L);
                    yR = (f1 ? filters[0][1].process (x1R, t1) : x1R) + (f2 ? filters[1][1].process (x2R, t2) : x2R);
                    break;
                }
                default: // Serial
                {
                    float xL = aL + bL + subS + nL, xR = aR + bR + subS + nR;
                    if (f1) { xL = filters[0][0].process (xL, t1); xR = filters[0][1].process (xR, t1); }
                    if (f2) { xL = filters[1][0].process (xL, t2); xR = filters[1][1].process (xR, t2); }
                    yL = xL; yR = xR;
                    break;
                }
            }

            // amplifier
            const float velAmp = lerp (1.0f, velocity, p.ampVelocity);
            const float amp = envLevel[0] * clampf (p.ampLevel + modCur[(size_t) ModDest::AmpLevel], 0.0f, 1.5f) * velAmp;
            float pl, pr; panGains (p.ampPan + modCur[(size_t) ModDest::AmpPan], pl, pr);
            outL[s] += yL * amp * pl * 1.4142f;
            outR[s] += yR * amp * pr * 1.4142f;

            if (! envs[0].isActive())
            {
                active = false;
                break;
            }
        }
    }

private:
    float sr = 48000.0f;
    const WavetableBank* bank = nullptr;
    Random rng;
    std::array<Oscillator, kNumOscs> osc;
    SubOscillator sub;
    NoiseGenerator noise;
    std::array<std::array<Filter, 2>, kNumFilters> filters; // [filter][channel]
    std::array<Envelope, kNumEnvs> envs;
    std::array<LFO, kNumLfos> lfos;

    bool active = false, releasedFlag = false;
    int note = 60; float velocity = 1.0f; uint64_t startStamp = 0;
    float currentNote = 60.0f, targetNote = 60.0f, glideCoef = 0.0f;
    float randomValue = 0.0f, keyTrackValue = 0.0f;
    int controlCounter = 0;
    std::array<float, kNumEnvs> envLevel { 0, 0, 0 };
    std::array<float, kNumLfos> lastLfo { 0, 0, 0 };
    std::array<float, (size_t) ModDest::NumDests> modCur {}, modNext {}, modStep {};

    inline float sourceValue (ModSource src, const GlobalModState& gm) const
    {
        switch (src)
        {
            case ModSource::Env1: return envLevel[0];
            case ModSource::Env2: return envLevel[1];
            case ModSource::Env3: return envLevel[2];
            case ModSource::Lfo1: return lastLfo[0];
            case ModSource::Lfo2: return lastLfo[1];
            case ModSource::Lfo3: return lastLfo[2];
            case ModSource::Velocity: return velocity;
            case ModSource::ModWheel: return gm.modWheel;
            case ModSource::Aftertouch: return gm.aftertouch;
            case ModSource::KeyTrack: return keyTrackValue;
            case ModSource::PitchBend: return gm.pitchBend;
            case ModSource::Random: return randomValue;
            case ModSource::Macro1: return gm.macros[0];
            case ModSource::Macro2: return gm.macros[1];
            case ModSource::Macro3: return gm.macros[2];
            case ModSource::Macro4: return gm.macros[3];
            default: return 0.0f;
        }
    }

    void evaluateModulation (const SynthParams& p, const GlobalModState& gm, int)
    {
        modNext.fill (0.0f);
        for (const auto& slot : p.mod)
        {
            if (slot.source == ModSource::None || slot.dest == ModDest::None || slot.amount == 0.0f) continue;
            const float v = sourceValue (slot.source, gm) * slot.amount;
            modNext[(size_t) slot.dest] += v;
        }
        // "All" destinations fan out
        modNext[(size_t) ModDest::OscAPitch] += 0.0f; // (PitchAll handled at use site)
    }

    void updateFilterTargets (const SynthParams& p)
    {
        const float env2 = envLevel[1] * clampf (1.0f + modNext[(size_t) ModDest::Env2Amount], 0.0f, 2.0f);
        const float cutAll = modNext[(size_t) ModDest::CutoffAll];
        for (int f = 0; f < kNumFilters; ++f)
        {
            const auto& fp = p.filter[(size_t) f];
            const float modOct = (f == 0 ? modNext[(size_t) ModDest::Filter1Cutoff] : modNext[(size_t) ModDest::Filter2Cutoff]) + cutAll;
            const float keyOct = fp.keyTrack * ((float) note - 60.0f) / 12.0f;
            const float envOct = fp.envAmount * 5.0f * env2;
            const float cutoff = fp.cutoff * std::exp2 (keyOct + envOct + modOct * 5.0f);
            const float res = fp.resonance + (f == 0 ? modNext[(size_t) ModDest::Filter1Res] : modNext[(size_t) ModDest::Filter2Res]);
            const float drive = fp.drive + modNext[(size_t) ModDest::FilterDrive];
            filters[(size_t) f][0].setTargets (cutoff, res, drive);
            filters[(size_t) f][1].setTargets (cutoff, res, drive);
        }
    }
};

} // namespace lucid
