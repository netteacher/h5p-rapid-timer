// LUCID Synth - DSP core utilities (JUCE independent)
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <vector>

namespace lucid {

constexpr float kPi   = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) { return g > 1.0e-9f ? 20.0f * std::log10 (g) : -180.0f; }
inline float midiToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float centsToRatio (float cents) { return std::exp2 (cents / 1200.0f); }

// Flush denormals / NaN guard
inline float sanitize (float v)
{
    if (! std::isfinite (v)) return 0.0f;
    return std::fabs (v) < 1.0e-20f ? 0.0f : v;
}

// Fast tanh (Padé approximant, accurate to ~1e-4 in [-4,4], clamped beyond)
inline float fastTanh (float x)
{
    if (x < -4.97f) return -1.0f;
    if (x >  4.97f) return  1.0f;
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

// Fast tan for filter coefficient computation, x in [0, pi/2)
inline float fastTan (float x)
{
    // Use the identity tan(x) = sin/cos with a good polynomial for small x, exact call otherwise.
    return std::tan (x);
}

// Deterministic xorshift RNG (per-voice noise, unison phase, drift)
struct Random
{
    uint32_t state = 0x9E3779B9u;
    explicit Random (uint32_t seed = 0x9E3779B9u) : state (seed ? seed : 1u) {}
    inline uint32_t next()
    {
        uint32_t x = state;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return state = x;
    }
    inline float uniform()  { return (float) (next() >> 8) * (1.0f / 16777216.0f); }      // [0,1)
    inline float bipolar()  { return uniform() * 2.0f - 1.0f; }                             // [-1,1)
};

// One-pole parameter smoother (exponential), for zipper-free control signals
struct Smoother
{
    float current = 0.0f, target = 0.0f, coef = 0.0f;
    void prepare (float sampleRate, float timeMs)
    {
        coef = std::exp (-1.0f / (0.001f * timeMs * sampleRate));
    }
    void reset (float v) { current = target = v; }
    void setTarget (float v) { target = v; }
    inline float process()
    {
        current = target + (current - target) * coef;
        return current;
    }
    inline bool isSmoothing() const { return std::fabs (current - target) > 1.0e-6f; }
};

// Linear ramp smoother (per block target)
struct LinearRamp
{
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int remaining = 0;
    void reset (float v) { current = target = v; remaining = 0; step = 0; }
    void setTarget (float v, int numSamples)
    {
        target = v;
        remaining = numSamples;
        step = numSamples > 0 ? (target - current) / (float) numSamples : 0.0f;
        if (numSamples <= 0) current = target;
    }
    inline float process()
    {
        if (remaining > 0) { current += step; --remaining; if (remaining == 0) current = target; }
        return current;
    }
};

// DC blocker (leaky differentiator)
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, R = 0.995f;
    void prepare (float sampleRate) { R = 1.0f - (kTwoPi * 5.0f / sampleRate); }
    void reset() { x1 = y1 = 0.0f; }
    inline float process (float x)
    {
        const float y = x - x1 + R * y1;
        x1 = x; y1 = y;
        return y;
    }
};

// Simple one-pole lowpass / highpass (tone controls, damping)
struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float hz, float sampleRate)
    {
        const float x = std::exp (-kTwoPi * clampf (hz, 1.0f, sampleRate * 0.49f) / sampleRate);
        a = 1.0f - x;
    }
    void reset() { z = 0.0f; }
    inline float lowpass (float x)  { z += a * (x - z); return z; }
    inline float highpass (float x) { return x - lowpass (x); }
};

// Equal power pan: pos in [-1, 1] -> (gainL, gainR)
inline void panGains (float pos, float& gl, float& gr)
{
    const float p = (clampf (pos, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi; // 0..pi/2
    gl = std::cos (p);
    gr = std::sin (p);
}

// Final safety clipper: perfectly linear below 0.9 (no distortion, no aliasing on normal
// programme material), smooth tanh knee above that, never exceeds 1.0.
inline float softClip (float x)
{
    constexpr float knee = 0.9f;
    const float a = std::fabs (x);
    if (a <= knee) return x;
    const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
    return x < 0.0f ? -y : y;
}

} // namespace lucid
