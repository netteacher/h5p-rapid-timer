// LUCID PULSE - synthesised drum voices (kick, snare, clap, closed/open hat, perc, rim, noise).
// Every voice is a small analogue-style model with exponential envelopes; all nonlinear stages are
// soft so the kit stays clean at high levels. No JUCE dependency.
#pragma once
#include "../../Source/dsp/Core.h"
#include "../../Source/dsp/Filters.h"
#include <array>

namespace pulse {

enum class VoiceType : int { Kick = 0, Snare, Clap, ClosedHat, OpenHat, Perc, Rim, Noise };
inline const char* voiceName (int i)
{
    static const char* n[8] = { "KICK", "SNARE", "CLAP", "CL HAT", "OP HAT", "PERC", "RIM", "NOISE" };
    return n[i < 0 ? 0 : (i > 7 ? 7 : i)];
}
// Labels of the three voice-specific controls per type
inline const char* voiceCtlName (int type, int ctl)
{
    static const char* n[8][3] = {
        { "PUNCH", "CLICK", "DRIVE" }, { "SNAP", "BODY", "NOISE" }, { "SPREAD", "TAIL", "BRIGHT" }, { "METAL", "SIZZLE", "CUT" },
        { "METAL", "SIZZLE", "CUT" }, { "BEND", "BODY", "NOISE" }, { "RING", "SNAP", "COLOR" }, { "COLOR", "SWEEP", "RES" } };
    return n[type < 0 ? 0 : (type > 7 ? 7 : type)][ctl < 0 ? 0 : (ctl > 2 ? 2 : ctl)];
}

struct VoiceParams
{
    float level = 0.8f;   // 0..1
    float pan = 0.0f;     // -1..1
    float tune = 0.0f;    // semitones -24..24
    float decay = 0.5f;   // 0..1
    float tone = 0.5f;    // 0..1
    float ctl[3] = { 0.5f, 0.5f, 0.5f };
};

// exponential decay envelope: 1 -> 0 with time constant
struct DecayEnv
{
    float v = 0.0f, coef = 0.0f;
    void trigger (float seconds, float sr) { v = 1.0f; coef = std::exp (-1.0f / (std::max (0.001f, seconds) * sr)); }
    inline float process() { v *= coef; return v; }
    bool active() const { return v > 1.0e-4f; }
};

class DrumVoice
{
public:
    void prepare (float sampleRate, VoiceType t, uint32_t seed)
    {
        sr = sampleRate; type = t; rng = lucid::Random (seed);
        svf.reset(); hp.reset(); dc.prepare (sr); dc.reset();
        for (auto& p : metalPhase) p = rng.uniform();
        active = false;
    }

    void trigger (const VoiceParams& p, float velocity)
    {
        params = p; vel = velocity; active = true; t = 0;
        const float d = p.decay;
        switch (type)
        {
            case VoiceType::Kick:
                ampEnv.trigger (0.08f + d * d * 1.2f, sr); pitchEnv.trigger (0.01f + p.ctl[0] * 0.09f, sr); clickEnv.trigger (0.004f, sr);
                phase = 0.0f; break;
            case VoiceType::Snare:
                ampEnv.trigger (0.05f + d * 0.35f, sr); noiseEnv.trigger (0.06f + d * 0.3f, sr); pitchEnv.trigger (0.02f, sr); phase = 0.0f; phase2 = 0.0f; break;
            case VoiceType::Clap:
                ampEnv.trigger (0.08f + d * 0.6f, sr); burst = 0; burstTimer = 0; clickEnv.trigger (0.008f, sr); break;
            case VoiceType::ClosedHat:
                ampEnv.trigger (0.02f + d * 0.15f, sr); break;
            case VoiceType::OpenHat:
                ampEnv.trigger (0.15f + d * 0.9f, sr); break;
            case VoiceType::Perc:
                ampEnv.trigger (0.05f + d * 0.6f, sr); pitchEnv.trigger (0.02f + p.ctl[0] * 0.2f, sr); noiseEnv.trigger (0.02f, sr); phase = 0.0f; break;
            case VoiceType::Rim:
                ampEnv.trigger (0.01f + d * 0.12f, sr); clickEnv.trigger (0.003f, sr); phase = 0.0f; break;
            default:
                ampEnv.trigger (0.05f + d * 1.5f, sr); pitchEnv.trigger (0.05f + d * 1.5f, sr); break;
        }
    }
    void choke() { if (active) ampEnv.coef = std::exp (-1.0f / (0.004f * sr)); }
    bool isActive() const { return active; }

    inline void process (float& outL, float& outR)
    {
        if (! active) { outL = outR = 0.0f; return; }
        const float baseHz = 55.0f * std::exp2 (params.tune / 12.0f);
        float s = 0.0f;
        const auto& c = params.ctl;
        switch (type)
        {
            case VoiceType::Kick:
            {
                const float pe = pitchEnv.process();
                const float hz = baseHz * (1.0f + pe * (2.0f + c[0] * 10.0f));
                phase += hz / sr; if (phase >= 1.0f) phase -= 1.0f;
                float body = std::sin (lucid::kTwoPi * phase);
                body = lucid::fastTanh (body * (1.0f + c[2] * 6.0f)) / lucid::fastTanh (1.0f + c[2] * 6.0f) * (1.0f + c[2] * 0.5f);
                const float click = clickEnv.process() * rng.bipolar() * c[1] * 1.5f;
                s = (body + click) * ampEnv.process();
                svf.set (lucid::lerp (200.0f, 6000.0f, params.tone), 0.1f, sr);
                float lp, bp, hpv; svf.process (s, lp, bp, hpv); s = lp;
                break;
            }
            case VoiceType::Snare:
            {
                const float pe = pitchEnv.process();
                const float hz1 = baseHz * 3.3f * (1.0f + pe * 1.5f), hz2 = hz1 * 1.72f;
                phase += hz1 / sr; if (phase >= 1.0f) phase -= 1.0f;
                phase2 += hz2 / sr; if (phase2 >= 1.0f) phase2 -= 1.0f;
                const float body = (std::sin (lucid::kTwoPi * phase) + 0.6f * std::sin (lucid::kTwoPi * phase2)) * ampEnv.process() * c[1];
                svf.set (lucid::lerp (1500.0f, 9000.0f, params.tone), 0.25f, sr);
                float lp, bp, hpv; svf.process (rng.bipolar(), lp, bp, hpv);
                const float noise = (bp * 2.0f + hpv * 0.5f) * noiseEnv.process() * (0.3f + c[2]);
                s = body + noise * (0.5f + c[0]);
                break;
            }
            case VoiceType::Clap:
            {
                // 3 short bursts then a tail
                float env;
                if (burst < 3)
                {
                    const int spacing = (int) (sr * (0.008f + c[0] * 0.012f));
                    env = std::exp (-(float) burstTimer / (sr * 0.004f));
                    if (++burstTimer >= spacing) { burstTimer = 0; ++burst; }
                    if (burst == 3) ampEnv.trigger (0.05f + params.decay * 0.5f * (0.5f + c[1]), sr);
                    ampEnv.v = 1.0f;
                }
                else env = ampEnv.process() * (0.6f + c[1] * 0.4f);
                svf.set (lucid::lerp (900.0f, 3000.0f, params.tone) * (1.0f + c[2]), 0.35f, sr);
                float lp, bp, hpv; svf.process (rng.bipolar(), lp, bp, hpv);
                s = bp * 2.5f * env;
                break;
            }
            case VoiceType::ClosedHat:
            case VoiceType::OpenHat:
            {
                // six square oscillators (808-style inharmonic stack) + noise, high-passed
                static const float ratios[6] = { 1.0f, 1.3420f, 1.5218f, 1.6667f, 1.9048f, 2.4762f };
                const float f0 = 320.0f * std::exp2 (params.tune / 12.0f) * (0.7f + c[0] * 0.8f);
                float metal = 0.0f;
                for (int i = 0; i < 6; ++i)
                {
                    metalPhase[(size_t) i] += f0 * ratios[i] / sr; if (metalPhase[(size_t) i] >= 1.0f) metalPhase[(size_t) i] -= 1.0f;
                    metal += metalPhase[(size_t) i] < 0.5f ? 1.0f : -1.0f;
                }
                metal *= 0.16f;
                const float mix = lucid::lerp (metal, rng.bipolar(), c[1]);
                hp.set (lucid::lerp (3000.0f, 9000.0f, c[2]), 0.2f, sr);
                float lp, bp, hpv; hp.process (mix, lp, bp, hpv);
                svf.set (lucid::lerp (6000.0f, 16000.0f, params.tone), 0.1f, sr);
                float lp2, bp2, hp2; svf.process (hpv, lp2, bp2, hp2);
                s = lp2 * ampEnv.process() * 1.5f;
                break;
            }
            case VoiceType::Perc:
            {
                const float pe = pitchEnv.process();
                const float hz = baseHz * 4.0f * (1.0f + pe * c[0] * 3.0f);
                phase += hz / sr; if (phase >= 1.0f) phase -= 1.0f;
                const float body = std::sin (lucid::kTwoPi * phase) + 0.3f * std::sin (lucid::kTwoPi * phase * 2.0f) * c[1];
                const float n = rng.bipolar() * noiseEnv.process() * c[2];
                s = (body + n) * ampEnv.process();
                svf.set (lucid::lerp (600.0f, 8000.0f, params.tone), 0.15f, sr);
                float lp, bp, hpv; svf.process (s, lp, bp, hpv); s = lp;
                break;
            }
            case VoiceType::Rim:
            {
                const float hz = baseHz * 8.0f * (0.6f + c[2] * 0.8f);
                phase += hz / sr; if (phase >= 1.0f) phase -= 1.0f;
                const float ring = std::sin (lucid::kTwoPi * phase) * (0.3f + c[0] * 0.7f);
                const float click = clickEnv.process() * rng.bipolar() * (0.5f + c[1]);
                svf.set (lucid::lerp (1200.0f, 5000.0f, params.tone), 0.6f, sr);
                float lp, bp, hpv; svf.process (ring + click, lp, bp, hpv);
                s = bp * 2.0f * ampEnv.process();
                break;
            }
            default: // Noise: filtered noise with sweep
            {
                const float pe = pitchEnv.process();
                const float sweep = (c[1] - 0.5f) * 2.0f; // -1..1 (down / up)
                const float fc = lucid::lerp (300.0f, 12000.0f, params.tone) * std::exp2 (sweep * (1.0f - pe) * 3.0f);
                svf.set (lucid::clampf (fc, 60.0f, 18000.0f), 0.1f + c[2] * 0.8f, sr);
                float lp, bp, hpv; svf.process (rng.bipolar(), lp, bp, hpv);
                s = lucid::lerp (lp, hpv, c[0]) * ampEnv.process() * 0.8f;
                break;
            }
        }
        static const float trim[8] = { 0.62f, 0.5f, 0.7f, 1.3f, 1.3f, 0.75f, 0.7f, 1.0f };
        s = dc.process (s) * vel * params.level * params.level * 1.5f * trim[(int) type];
        float gl, gr; lucid::panGains (params.pan, gl, gr);
        outL = s * gl * 1.4142f; outR = s * gr * 1.4142f;
        if (! ampEnv.active() && (type != VoiceType::Clap || burst >= 3)) active = false;
        ++t;
    }

private:
    float sr = 48000.0f;
    VoiceType type = VoiceType::Kick;
    VoiceParams params;
    lucid::Random rng;
    lucid::SvfStage svf, hp;
    lucid::DcBlocker dc;
    DecayEnv ampEnv, pitchEnv, noiseEnv, clickEnv;
    float phase = 0.0f, phase2 = 0.0f, vel = 1.0f;
    std::array<float, 6> metalPhase {};
    int burst = 0, burstTimer = 0, t = 0;
    bool active = false;
};

class DrumKit
{
public:
    void prepare (float sampleRate)
    {
        for (int i = 0; i < 8; ++i) voices[(size_t) i].prepare (sampleRate, (VoiceType) i, 77u + (uint32_t) i * 31u);
    }
    void trigger (int lane, const VoiceParams& p, float velocity)
    {
        if (lane < 0 || lane >= 8) return;
        if (lane == (int) VoiceType::ClosedHat) voices[(size_t) VoiceType::OpenHat].choke(); // hat choke group
        voices[(size_t) lane].trigger (p, velocity);
    }
    inline void process (float& l, float& r)
    {
        l = r = 0.0f;
        for (auto& v : voices) { float a, b; v.process (a, b); l += a; r += b; }
    }
    bool anyActive() const { for (const auto& v : voices) if (v.isActive()) return true; return false; }
private:
    std::array<DrumVoice, 8> voices;
};

} // namespace pulse
