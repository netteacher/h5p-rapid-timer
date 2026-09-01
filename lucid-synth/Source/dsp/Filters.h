// LUCID Synth - filters: TPT/ZDF state variable filter (Cytomic) and a Huovilainen-style
// transistor ladder with 2x oversampling. Both are zero-delay-feedback designs which stay
// clean and stable under fast modulation.
#pragma once
#include "Core.h"
#include "Params.h"

namespace lucid {

// Second-order state variable (trapezoidal integration). 12 dB/oct multi-mode.
struct SvfStage
{
    float ic1eq = 0.0f, ic2eq = 0.0f;
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    void reset() { ic1eq = ic2eq = 0.0f; }
    inline void set (float cutoffHz, float res, float sr)
    {
        const float fc = clampf (cutoffHz, 10.0f, sr * 0.45f);
        g = std::tan (kPi * fc / sr);
        k = 2.0f - 1.96f * clampf (res, 0.0f, 1.0f); // k -> 0.04 at full resonance (strong but bounded)
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // returns lp, bp, hp for the input sample
    inline void process (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
};

// Huovilainen transistor ladder (4 x one-pole with tanh, 2x oversampled)
struct LadderCore
{
    float s[4] = { 0, 0, 0, 0 };
    float y4prev = 0.0f;
    float g = 0.0f, k = 0.0f;
    static constexpr float kVt = 1.2f; // thermal voltage scaling (larger = cleaner)

    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; y4prev = 0.0f; }
    inline void set (float cutoffHz, float res, float srOversampled)
    {
        const float fc = clampf (cutoffHz, 10.0f, srOversampled * 0.22f);
        const float x = kPi * fc / srOversampled;
        // Huovilainen's tuned cutoff (compensates for the nonlinear one-pole)
        g = 1.0f - std::exp (-2.0f * x) ;
        k = 4.0f * clampf (res, 0.0f, 1.0f) * 0.98f;
    }
    inline float process (float in)
    {
        const float x = in - k * y4prev;
        float u = fastTanh (x / kVt);
        for (int i = 0; i < 4; ++i)
        {
            const float t = fastTanh (s[i] / kVt);
            s[i] += g * (u - t) * kVt;
            u = fastTanh (s[i] / kVt);
        }
        y4prev = s[3];
        return s[3];
    }
};

// Polyphase half-band for 2x oversampling (steep, 12th order IIR allpass network)
struct HalfBand2x
{
    struct AllpassChain
    {
        static constexpr int N = 6;
        float a[N]; float x1[N] = {}, y1[N] = {};
        void reset() { for (int i = 0; i < N; ++i) x1[i] = y1[i] = 0.0f; }
        inline float process (float in)
        {
            float x = in;
            for (int i = 0; i < N; ++i)
            {
                const float y = a[i] * (x - y1[i]) + x1[i];
                x1[i] = x; y1[i] = y; x = y;
            }
            return x;
        }
    };
    AllpassChain upA, upB, downA, downB;
    float lastUp = 0.0f;

    HalfBand2x()
    {
        // Elliptic polyphase design (Valenzuela/Constantinides), 12 coefficients, transition band 0.01 fs:
        // > 104 dB stop-band rejection, < 1e-9 dB pass-band ripple.
        static const float ca[6] = { 0.036681502163648f, 0.2746317593794541f, 0.5610986978791948f, 0.7697418338632266f, 0.8922608180038789f, 0.962094548378084f };
        static const float cb[6] = { 0.1365476246319577f, 0.4231386174365667f, 0.6775400499741616f, 0.839889624849638f, 0.9315419599631839f, 0.9878163707328971f };
        for (int i = 0; i < 6; ++i) { upA.a[i] = downA.a[i] = ca[i]; upB.a[i] = downB.a[i] = cb[i]; }
    }
    void reset() { upA.reset(); upB.reset(); downA.reset(); downB.reset(); lastUp = 0.0f; }

    // Upsample one input into two outputs
    inline void up (float in, float& o0, float& o1)
    {
        // Polyphase interpolation: both branches fed with the same sample
        o0 = upA.process (in);
        o1 = upB.process (in);
    }
    // Downsample two consecutive inputs (i0 = even, i1 = odd) into one output.
    // H(z) = 0.5 * (A(z^2) + z^-1 B(z^2)): the delayed branch sees the earlier sample.
    inline float down (float i0, float i1)
    {
        return 0.5f * (downA.process (i1) + downB.process (i0));
    }
};

class Filter
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        cutoffSmooth.prepare (sr, 4.0f); cutoffSmooth.reset (8000.0f);
        resSmooth.prepare (sr, 8.0f);    resSmooth.reset (0.0f);
        driveSmooth.prepare (sr, 10.0f); driveSmooth.reset (0.0f);
        reset();
    }
    void reset()
    {
        svf1.reset(); svf2.reset(); ladder.reset(); os.reset(); dc.reset();
        dc.prepare (sr);
    }
    void resetTo (float cutoffHz, float res, float drive)
    {
        cutoffSmooth.reset (cutoffHz); resSmooth.reset (res); driveSmooth.reset (drive);
    }
    inline void setTargets (float cutoffHz, float res, float drive)
    {
        cutoffSmooth.setTarget (clampf (cutoffHz, 20.0f, 20000.0f));
        resSmooth.setTarget (clampf (res, 0.0f, 1.0f));
        driveSmooth.setTarget (clampf (drive, 0.0f, 1.0f));
    }

    inline float process (float in, FilterType type)
    {
        const float fc = cutoffSmooth.process();
        const float res = resSmooth.process();
        const float drive = driveSmooth.process();
        const float preGain = 1.0f + drive * 5.0f;
        float x = in * preGain;
        if (drive > 0.001f) x = fastTanh (x);   // input saturation stage
        const float makeup = 1.0f / (1.0f + drive * 1.6f);

        if (type == FilterType::Ladder)
        {
            ladder.set (fc, res, sr * 2.0f);
            float a, b;
            os.up (x, a, b);
            a = ladder.process (a);
            b = ladder.process (b);
            float y = os.down (a, b);
            // passband gain compensation for the resonance-induced loss
            y *= 1.0f + res * 1.2f;
            return dc.process (y) * makeup;
        }

        svf1.set (fc, res, sr);
        float lp, bp, hp;
        svf1.process (x, lp, bp, hp);
        float y;
        switch (type)
        {
            case FilterType::LP12:  y = lp; break;
            case FilterType::HP12:  y = hp; break;
            case FilterType::BP12:  y = bp * svf1.k * 1.5f; break; // normalised BP
            case FilterType::Notch: y = x - svf1.k * bp; break;
            case FilterType::LP24:
            {
                svf2.set (fc, res * 0.75f, sr);
                float lp2, bp2, hp2; svf2.process (lp, lp2, bp2, hp2); y = lp2; break;
            }
            case FilterType::HP24:
            {
                svf2.set (fc, res * 0.75f, sr);
                float lp2, bp2, hp2; svf2.process (hp, lp2, bp2, hp2); y = hp2; break;
            }
            default: y = lp; break;
        }
        // Gentle soft limiting keeps self-oscillation musical instead of harsh
        if (res > 0.7f) y = fastTanh (y * 0.8f) * 1.25f;
        return y * makeup;
    }

    // Magnitude response (for the UI curve), in dB at frequency hz for given settings
    static float magnitudeDb (FilterType type, float cutoffHz, float res, float hz, float sampleRate)
    {
        // Use analogue prototype responses (close enough for display, exact for the ZDF designs at low f)
        const float w = hz / clampf (cutoffHz, 10.0f, 20000.0f);
        const float k = type == FilterType::Ladder ? 0.0f : 2.0f - 1.96f * res;
        const float w2 = w * w;
        auto lp2 = [&] (float kk) { return 1.0f / std::sqrt ((1.0f - w2) * (1.0f - w2) + kk * kk * w2); };
        float mag = 1.0f;
        switch (type)
        {
            case FilterType::LP12:  mag = lp2 (k); break;
            case FilterType::HP12:  mag = w2 * lp2 (k); break;
            case FilterType::BP12:  mag = k * w * lp2 (k) * 1.5f; break;
            case FilterType::Notch: mag = std::fabs (1.0f - w2) * lp2 (k); break;
            case FilterType::LP24:  mag = lp2 (k) * lp2 (2.0f - 1.96f * res * 0.75f); break;
            case FilterType::HP24:  mag = w2 * w2 * lp2 (k) * lp2 (2.0f - 1.96f * res * 0.75f); break;
            case FilterType::Ladder:
            {
                // 4-pole with feedback: H = G^4 / (1 + kres G^4), G = 1/(1 + jw)
                const float kres = 4.0f * res * 0.98f;
                const float g2 = 1.0f / (1.0f + w2);
                // complex arithmetic
                float re = 1.0f, im = 0.0f; // G^4 numerator computed via (1 - jw)^4 / (1+w2)^4
                const float ar = 1.0f, ai = -w;
                float pr = ar, pi = ai;
                for (int i = 0; i < 3; ++i) { const float nr = pr * ar - pi * ai; const float ni = pr * ai + pi * ar; pr = nr; pi = ni; }
                re = pr * g2 * g2; im = pi * g2 * g2;
                const float dr = 1.0f + kres * re, di = kres * im;
                const float dmag = dr * dr + di * di;
                mag = std::sqrt ((re * re + im * im) / std::max (dmag, 1.0e-12f)) * (1.0f + res * 1.2f);
                break;
            }
            default: break;
        }
        (void) sampleRate;
        return gainToDb (std::max (mag, 1.0e-6f));
    }

private:
    float sr = 48000.0f;
    SvfStage svf1, svf2;
    LadderCore ladder;
    HalfBand2x os;
    DcBlocker dc;
    Smoother cutoffSmooth, resSmooth, driveSmooth;
};

} // namespace lucid
