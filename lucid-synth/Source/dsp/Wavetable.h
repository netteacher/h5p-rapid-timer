// LUCID Synth - band-limited, mip-mapped wavetables with frame morphing.
// Each table = numFrames frames; each frame stored in kNumMips band-limited versions
// (one per octave of fundamental frequency), built through the FFT so no harmonic
// above Nyquist ever survives -> alias-free "ultra clear" oscillators.
#pragma once
#include "Core.h"
#include "FFT.h"
#include <functional>
#include <string>
#include <memory>
#include <complex>

namespace lucid {

class Wavetable
{
public:
    static constexpr int kFrameSize = 2048;
    static constexpr int kNumMips   = 10;               // fundamentals 20 Hz .. 20 kHz in octave bands
    static constexpr int kStride    = kFrameSize + 1;   // + wrap sample for interpolation
    static constexpr float kLowestHz = 20.0f;

    using cpx = std::complex<float>;
    // Fills 'spectrum' (kFrameSize/2 + 1 bins, bin h = harmonic h) for a given frame.
    using SpectrumGenerator = std::function<void (int frame, int numFrames, std::vector<cpx>& spectrum)>;
    // Time-domain generator: phase in [0,1), morph in [0,1] -> sample.
    using TimeGenerator = std::function<float (float phase, float morph)>;

    std::string name;
    int numFrames = 1;

    void buildFromSpectrum (const std::string& tableName, int frames, float sampleRate, const SpectrumGenerator& gen)
    {
        name = tableName;
        numFrames = std::max (1, frames);
        samples.assign ((size_t) numFrames * kNumMips * kStride, 0.0f);
        FFT fft (kFrameSize);
        std::vector<cpx> spec ((size_t) kFrameSize / 2 + 1);
        std::vector<cpx> buf ((size_t) kFrameSize);

        for (int fr = 0; fr < numFrames; ++fr)
        {
            std::fill (spec.begin(), spec.end(), cpx (0.0f, 0.0f));
            gen (fr, numFrames, spec);
            spec[0] = cpx (0.0f, 0.0f); // no DC

            float peak = 0.0f;
            for (int mip = 0; mip < kNumMips; ++mip)
            {
                const int maxHarm = maxHarmonicForMip (mip, sampleRate);
                std::fill (buf.begin(), buf.end(), cpx (0.0f, 0.0f));
                for (int h = 1; h <= maxHarm && h < kFrameSize / 2; ++h)
                {
                    buf[(size_t) h] = spec[(size_t) h];
                    buf[(size_t) (kFrameSize - h)] = std::conj (spec[(size_t) h]);
                }
                fft.perform (buf, true);
                float* dst = ptr (fr, mip);
                for (int i = 0; i < kFrameSize; ++i) dst[i] = buf[(size_t) i].real();
                if (mip == 0)
                {
                    for (int i = 0; i < kFrameSize; ++i) peak = std::max (peak, std::fabs (dst[i]));
                }
                const float norm = peak > 1.0e-9f ? 1.0f / peak : 1.0f;
                for (int i = 0; i < kFrameSize; ++i) dst[i] *= norm;
                dst[kFrameSize] = dst[0];
            }
        }
    }

    void buildFromTime (const std::string& tableName, int frames, float sampleRate, const TimeGenerator& gen)
    {
        FFT fft (kFrameSize);
        std::vector<cpx> buf ((size_t) kFrameSize);
        buildFromSpectrum (tableName, frames, sampleRate, [&] (int frame, int nFrames, std::vector<cpx>& spec)
        {
            const float morph = nFrames > 1 ? (float) frame / (float) (nFrames - 1) : 0.0f;
            for (int i = 0; i < kFrameSize; ++i)
                buf[(size_t) i] = cpx (gen ((float) i / (float) kFrameSize, morph), 0.0f);
            fft.perform (buf, false);
            const float scale = 1.0f / (float) kFrameSize;
            for (int h = 0; h <= kFrameSize / 2; ++h) spec[(size_t) h] = buf[(size_t) h] * scale;
        });
    }

    static int maxHarmonicForMip (int mip, float sampleRate)
    {
        const float topFundamental = kLowestHz * std::exp2 ((float) (mip + 1));
        const int h = (int) std::floor ((sampleRate * 0.5f) / topFundamental) - 1;
        return std::max (1, std::min (h, kFrameSize / 2 - 1));
    }

    // Continuous mip position for a fundamental frequency (0 .. kNumMips-1)
    static inline float mipPosition (float hz)
    {
        const float p = std::log2 (std::max (hz, kLowestHz) / kLowestHz);
        return clampf (p, 0.0f, (float) (kNumMips - 1));
    }

    inline const float* ptr (int frame, int mip) const { return samples.data() + ((size_t) frame * kNumMips + (size_t) mip) * kStride; }
    inline float* ptr (int frame, int mip) { return samples.data() + ((size_t) frame * kNumMips + (size_t) mip) * kStride; }

    // Linear-interpolated read of a single frame/mip
    static inline float readTable (const float* t, float phase01)
    {
        const float fi = phase01 * (float) kFrameSize;
        int i = (int) fi;
        const float f = fi - (float) i;
        if (i >= kFrameSize) { i = kFrameSize - 1; }
        return t[i] + f * (t[i + 1] - t[i]);
    }

    // Full read: frame morph + mip crossfade
    inline float read (float phase01, float morph01, float mipPos) const
    {
        const float fpos = clampf (morph01, 0.0f, 1.0f) * (float) (numFrames - 1);
        int f0 = (int) fpos; if (f0 >= numFrames - 1) f0 = std::max (0, numFrames - 2);
        const float ff = numFrames > 1 ? fpos - (float) f0 : 0.0f;
        const int f1 = numFrames > 1 ? f0 + 1 : f0;

        int m0 = (int) mipPos; if (m0 >= kNumMips - 1) m0 = kNumMips - 2;
        const float mf = mipPos - (float) m0;
        const int m1 = m0 + 1;

        const float a = readTable (ptr (f0, m0), phase01);
        const float b = readTable (ptr (f1, m0), phase01);
        const float c = readTable (ptr (f0, m1), phase01);
        const float d = readTable (ptr (f1, m1), phase01);
        const float lo = a + ff * (b - a);
        const float hi = c + ff * (d - c);
        return lo + mf * (hi - lo);
    }

    bool isBuilt() const { return ! samples.empty(); }
    // Frame for display (mip 0)
    const float* displayFrame (float morph01) const
    {
        const int fr = (int) std::round (clampf (morph01, 0.0f, 1.0f) * (float) (numFrames - 1));
        return ptr (fr, 0);
    }

private:
    std::vector<float> samples;
};

// ---------------------------------------------------------------------------------------------
// Factory wavetable bank
// ---------------------------------------------------------------------------------------------
class WavetableBank
{
public:
    static constexpr int kNumTables = 14;
    static constexpr int kFrameSize = Wavetable::kFrameSize;

    static const char* tableName (int idx)
    {
        static const char* names[kNumTables] = {
            "Basic Shapes", "Analog Saw", "PWM", "Sync Saw", "Wavefold", "Vowel",
            "Overtones", "Glass", "Bell", "Bitcrush", "Spectral Sweep", "Phase Dist", "Grain Cloud", "Pure Sine"
        };
        return names[std::max (0, std::min (idx, kNumTables - 1))];
    }

    void build (float sampleRate)
    {
        if (builtRate == sampleRate && tables.size() == kNumTables) return;
        builtRate = sampleRate;
        tables.clear();
        tables.resize (kNumTables);
        for (int i = 0; i < kNumTables; ++i) buildTable (i, tables[(size_t) i], sampleRate);
        sawTable.buildFromSpectrum ("VA Saw", 1, sampleRate, [] (int, int, std::vector<std::complex<float>>& s)
        {
            for (int h = 1; h < kFrameSize / 2; ++h) s[(size_t) h] = std::complex<float> (0.0f, -(2.0f / kPi) / (float) h);
        });
        triTable.buildFromSpectrum ("VA Triangle", 1, sampleRate, [] (int, int, std::vector<std::complex<float>>& s)
        {
            for (int h = 1; h < kFrameSize / 2; h += 2)
                s[(size_t) h] = std::complex<float> (0.0f, -((((h - 1) / 2) & 1) ? -1.0f : 1.0f) * (8.0f / (kPi * kPi)) / (float) (h * h));
        });
    }

    const Wavetable& get (int idx) const { return tables[(size_t) std::max (0, std::min (idx, kNumTables - 1))]; }
    bool isBuilt() const { return tables.size() == kNumTables; }

    // Hidden single-frame tables used by the virtual-analogue engine (alias-free saw / triangle)
    const Wavetable& analogSaw() const { return sawTable; }
    const Wavetable& analogTriangle() const { return triTable; }

private:
    std::vector<Wavetable> tables;
    Wavetable sawTable, triTable;
    float builtRate = 0.0f;

    static void buildTable (int idx, Wavetable& wt, float sr)
    {
        using cpx = std::complex<float>;
        const int H = Wavetable::kFrameSize / 2;
        auto sawSpec = [] (int h) { return 1.0f / (float) h; };

        switch (idx)
        {
            case 0: // Basic Shapes: sine -> triangle -> saw -> square -> pulse
                wt.buildFromSpectrum ("Basic Shapes", 33, sr, [H] (int frame, int nFrames, std::vector<cpx>& s)
                {
                    const float m = (float) frame / (float) (nFrames - 1) * 4.0f; // 0..4
                    const int k = std::min (3, (int) m); const float t = m - (float) k;
                    auto shapeAmp = [] (int shape, int h) -> float {
                        switch (shape) {
                            case 0: return h == 1 ? 1.0f : 0.0f;
                            case 1: return (h & 1) ? ((((h - 1) / 2) & 1) ? -1.0f : 1.0f) * (8.0f / (kPi * kPi)) / (float) (h * h) : 0.0f;
                            case 2: return (2.0f / kPi) / (float) h;
                            case 3: return (h & 1) ? (4.0f / kPi) / (float) h : 0.0f;
                            default: { const float d = 0.25f; return (2.0f / (kPi * (float) h)) * std::sin (kPi * (float) h * d) * 2.0f; }
                        }
                    };
                    for (int h = 1; h < H; ++h)
                    {
                        const float a = lerp (shapeAmp (k, h), shapeAmp (k + 1, h), t);
                        s[(size_t) h] = cpx (0.0f, -a); // sine phase
                    }
                });
                break;

            case 1: // Analog Saw: brightness morph (harmonic roll-off)
                wt.buildFromSpectrum ("Analog Saw", 16, sr, [H, sawSpec] (int frame, int nFrames, std::vector<cpx>& s)
                {
                    const float m = (float) frame / (float) (nFrames - 1);
                    const float roll = 0.002f + m * m * 0.25f;
                    for (int h = 1; h < H; ++h) s[(size_t) h] = cpx (0.0f, -sawSpec (h) * std::exp (-roll * (float) (h - 1)));
                });
                break;

            case 2: // PWM 50% -> 3%
                wt.buildFromSpectrum ("PWM", 32, sr, [H] (int frame, int nFrames, std::vector<cpx>& s)
                {
                    const float m = (float) frame / (float) (nFrames - 1);
                    const float d = 0.5f - m * 0.47f;
                    for (int h = 1; h < H; ++h)
                    {
                        const float a = (2.0f / (kPi * (float) h)) * std::sin (kPi * (float) h * d);
                        const float ph = -kPi * (float) h * d;
                        s[(size_t) h] = cpx (a * std::cos (ph), a * std::sin (ph));
                    }
                });
                break;

            case 3: // Sync Saw (hard sync ratio 1 -> 6)
                wt.buildFromTime ("Sync Saw", 32, sr, [] (float p, float m)
                {
                    const float ratio = 1.0f + m * 5.0f;
                    const float q = p * ratio; const float x = q - std::floor (q);
                    const float env = 1.0f - p * 0.35f; // slight decay in the master period softens the reset click
                    return (2.0f * x - 1.0f) * env;
                });
                break;

            case 4: // Wavefold sine
                wt.buildFromTime ("Wavefold", 32, sr, [] (float p, float m)
                {
                    float x = std::sin (kTwoPi * p) * (1.0f + m * 7.0f);
                    // triangle fold into [-1,1]
                    x = x * 0.25f + 0.25f; x = x - std::floor (x); x = std::fabs (x * 4.0f - 2.0f) - 1.0f;
                    return x;
                });
                break;

            case 5: // Vowel: formant filtered saw, A E I O U
                wt.buildFromSpectrum ("Vowel", 33, sr, [H, sawSpec] (int frame, int nFrames, std::vector<cpx>& s)
                {
                    static const float F[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
                    const float m = (float) frame / (float) (nFrames - 1) * 4.0f;
                    const int k = std::min (3, (int) m); const float t = m - (float) k;
                    const float f0 = 110.0f;
                    for (int h = 1; h < H; ++h)
                    {
                        const float hz = f0 * (float) h;
                        float g = 0.0f;
                        for (int f = 0; f < 3; ++f)
                        {
                            const float fc = lerp (F[k][f], F[k + 1][f], t);
                            const float bw = 60.0f + fc * 0.08f;
                            const float d = (hz - fc) / bw;
                            g += std::exp (-0.5f * d * d) * (f == 0 ? 1.0f : (f == 1 ? 0.6f : 0.3f));
                        }
                        s[(size_t) h] = cpx (0.0f, -sawSpec (h) * (0.03f + g));
                    }
                });
                break;

            case 6: // Overtones: additive stack growing with morph
                wt.buildFromSpectrum ("Overtones", 16, sr, [] (int frame, int nFrames, std::vector<cpx>& s)
                {
                    static const int harm[10] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
                    const float m = (float) frame / (float) (nFrames - 1) * 9.0f;
                    for (int i = 0; i < 10; ++i)
                    {
                        const float w = clampf (m - (float) i + 1.0f, 0.0f, 1.0f);
                        const float a = w / std::sqrt ((float) harm[i]);
                        s[(size_t) harm[i]] = cpx (0.0f, -a);
                    }
                });
                break;

            case 7: // Glass: sparse odd harmonic combs with shifting spacing
                wt.buildFromSpectrum ("Glass", 24, sr, [H] (int frame, int nFrames, std::vector<cpx>& s)
                {
                    const float m = (float) frame / (float) (nFrames - 1);
                    const float spacing = 2.0f + m * 6.0f;
                    Random rng (1234u + (uint32_t) frame);
                    for (int h = 1; h < 200; ++h)
                    {
                        const float pos = (float) (h - 1) / spacing; const float fr = pos - std::floor (pos);
                        const float w = std::exp (-fr * fr * 30.0f) + std::exp (-(1.0f - fr) * (1.0f - fr) * 30.0f);
                        const float a = w / (float) h;
                        const float ph = rng.uniform() * kTwoPi;
                        s[(size_t) h] = cpx (a * std::cos (ph), a * std::sin (ph));
                    }
                });
                break;

            case 8: // Bell: sparse inharmonic-like partials with decaying brightness
                wt.buildFromSpectrum ("Bell", 16, sr, [] (int frame, int nFrames, std::vector<cpx>& s)
                {
                    static const int part[8] = { 1, 3, 5, 8, 12, 17, 23, 31 };
                    const float m = (float) frame / (float) (nFrames - 1);
                    for (int i = 0; i < 8; ++i)
                    {
                        const float a = (1.0f / (1.0f + (float) i * 0.6f)) * std::exp (-m * (float) i * 0.8f);
                        s[(size_t) part[i]] = cpx (0.0f, -a);
                    }
                });
                break;

            case 9: // Bitcrush: sine quantised to fewer levels
                wt.buildFromTime ("Bitcrush", 16, sr, [] (float p, float m)
                {
                    const float levels = 64.0f * std::exp2 (-m * 5.0f) ; // 64 .. 2 levels
                    const float x = std::sin (kTwoPi * p);
                    return std::round (x * levels * 0.5f) / (levels * 0.5f);
                });
                break;

            case 10: // Spectral Sweep: gaussian window over saw harmonics
                wt.buildFromSpectrum ("Spectral Sweep", 32, sr, [H, sawSpec] (int frame, int nFrames, std::vector<cpx>& s)
                {
                    const float m = (float) frame / (float) (nFrames - 1);
                    const float centre = 1.0f + m * m * 80.0f;
                    const float width = 2.0f + centre * 0.35f;
                    for (int h = 1; h < H; ++h)
                    {
                        const float d = ((float) h - centre) / width;
                        s[(size_t) h] = cpx (0.0f, -sawSpec (h) * (0.05f + std::exp (-0.5f * d * d)) * std::sqrt (centre));
                    }
                });
                break;

            case 11: // Phase distortion (CZ style)
                wt.buildFromTime ("Phase Dist", 32, sr, [] (float p, float m)
                {
                    const float d = 0.5f - m * 0.47f;
                    const float q = p < d ? p * (0.5f / d) : 0.5f + (p - d) * (0.5f / (1.0f - d));
                    return std::cos (kTwoPi * q);
                });
                break;

            case 12: // Grain Cloud: random-phase spectra, morph moves through random frames
                wt.buildFromSpectrum ("Grain Cloud", 16, sr, [] (int frame, int, std::vector<cpx>& s)
                {
                    Random rng (777u + (uint32_t) frame * 31u);
                    for (int h = 1; h < 160; ++h)
                    {
                        const float a = (0.3f + rng.uniform()) / std::sqrt ((float) h);
                        const float ph = rng.uniform() * kTwoPi;
                        s[(size_t) h] = cpx (a * std::cos (ph), a * std::sin (ph));
                    }
                });
                break;

            default: // Pure Sine
                wt.buildFromSpectrum ("Pure Sine", 1, sr, [] (int, int, std::vector<cpx>& s) { s[1] = cpx (0.0f, -1.0f); });
                break;
        }
    }
};

} // namespace lucid
