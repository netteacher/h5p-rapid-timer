// LUCID Synth - offline DSP tests (no JUCE). Renders audio through the engine and checks
// aliasing, stability, envelope timing, effects behaviour and voice allocation.
#include "../Source/dsp/SynthEngine.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace lucid;

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (!(cond)) { ++failures; std::printf ("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } else std::printf ("  ok:   %s\n", msg); } while (0)

static const float kSr = 48000.0f;

static bool allFinite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }
static float peakOf (const std::vector<float>& v) { float p = 0; for (float x : v) p = std::max (p, std::fabs (x)); return p; }
static float rmsOf (const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0) to = v.size();
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return (float) std::sqrt (s / (double) std::max<size_t> (1, to - from));
}

static void renderNote (SynthEngine& e, int note, float seconds, float holdSeconds, std::vector<float>& L, std::vector<float>& R)
{
    const int n = (int) (seconds * kSr), hold = (int) (holdSeconds * kSr);
    L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f);
    e.noteOn (note, 0.9f);
    const int block = 256;
    for (int pos = 0; pos < n; pos += block)
    {
        if (pos >= hold && pos - block < hold) e.noteOff (note);
        e.render (L.data() + pos, R.data() + pos, std::min (block, n - pos));
    }
}

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    std::ofstream f (path, std::ios::binary);
    if (! f) return;
    const uint32_t n = (uint32_t) L.size(); const uint16_t ch = 2, bits = 16; const uint32_t sr = (uint32_t) kSr;
    const uint32_t dataBytes = n * ch * bits / 8;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); }; auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVE", 4); f.write ("fmt ", 4); w32 (16); w16 (1); w16 (ch); w32 (sr); w32 (sr * ch * bits / 8); w16 (ch * bits / 8); w16 (bits);
    f.write ("data", 4); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i) { w16 ((uint16_t) (int16_t) (clampf (L[i], -1, 1) * 32767)); w16 ((uint16_t) (int16_t) (clampf (R[i], -1, 1) * 32767)); }
}

// Magnitude spectrum via the FFT used for wavetables
static std::vector<float> spectrum (const std::vector<float>& x, int size)
{
    FFT fft (size); std::vector<std::complex<float>> buf ((size_t) size);
    for (int i = 0; i < size; ++i)
    {
        const float t = kTwoPi * (float) i / (float) size; // 4-term Blackman-Harris: side lobes < -92 dB
        const float w = 0.35875f - 0.48829f * std::cos (t) + 0.14128f * std::cos (2 * t) - 0.01168f * std::cos (3 * t);
        buf[(size_t) i] = { x[(size_t) i] * w, 0.0f };
    }
    fft.perform (buf, false);
    std::vector<float> mag ((size_t) size / 2);
    for (int i = 0; i < size / 2; ++i) mag[(size_t) i] = std::abs (buf[(size_t) i]);
    return mag;
}

static SynthParams cleanParams()
{
    SynthParams p;
    p.osc[1].enabled = false; p.filter[0].enabled = false; p.filter[1].enabled = false;
    p.env[0].attack = 0.001f; p.env[0].release = 0.01f; p.env[0].sustain = 1.0f;
    p.fx.limiterOn = false;
    return p;
}

int main()
{
    std::printf ("LUCID DSP tests @ %.0f Hz\n", kSr);

    // ---------------------------------------------------------------- 1. basic render
    {
        std::printf ("[basic]\n");
        SynthEngine e; e.prepare (kSr, 256);
        SynthParams p; e.setParams (p);
        std::vector<float> L, R; renderNote (e, 60, 1.0f, 0.5f, L, R);
        CHECK (allFinite (L) && allFinite (R), "output is finite");
        CHECK (rmsOf (L, 1000, 20000) > 0.05f, "output has signal while note held");
        CHECK (rmsOf (L, 44000, 48000) < 1.0e-3f, "output decays after release");
        CHECK (peakOf (L) <= 1.0f, "limiter keeps peak <= 0 dBFS");
        CHECK (e.getActiveVoiceCount() == 0, "voice freed after release");
    }

    // ---------------------------------------------------------------- 2. aliasing of wavetable saw
    {
        std::printf ("[aliasing]\n");
        SynthEngine e; e.prepare (kSr, 256);
        SynthParams p = cleanParams();
        p.osc[0].engine = OscEngine::Wavetable; p.osc[0].table = 0; p.osc[0].morph = 0.5f; // saw frame
        p.osc[0].unison = 1;
        e.setParams (p);
        const int note = 96; // C7 = 2093 Hz
        std::vector<float> L, R; renderNote (e, note, 1.0f, 1.0f, L, R);
        const int N = 32768;
        std::vector<float> seg (L.begin() + 8000, L.begin() + 8000 + N);
        auto mag = spectrum (seg, N);
        const float f0 = midiToHz ((float) note); const float binHz = kSr / (float) N;
        double harmonicE = 0, otherE = 0;
        for (int b = 20; b < N / 2; ++b)
        {
            const float hz = (float) b * binHz;
            const float h = hz / f0; const float d = std::fabs (h - std::round (h)) * f0 / binHz; // distance in bins to nearest harmonic
            if (d < 8.0f) harmonicE += (double) mag[(size_t) b] * mag[(size_t) b]; else otherE += (double) mag[(size_t) b] * mag[(size_t) b];
        }
        const float ratioDb = (float) (10.0 * std::log10 (otherE / std::max (harmonicE, 1.0e-20)));
        std::printf ("  non-harmonic / harmonic energy: %.1f dB\n", ratioDb);
        CHECK (ratioDb < -70.0f, "wavetable saw at C7 is alias-free (< -70 dB)");

        // analog engine (PolyBLEP) should also be very clean
        p.osc[0].engine = OscEngine::Analog; p.osc[0].shape = AnalogShape::Saw; e.setParams (p);
        renderNote (e, note, 1.0f, 1.0f, L, R);
        std::vector<float> seg2 (L.begin() + 8000, L.begin() + 8000 + N);
        auto mag2 = spectrum (seg2, N);
        harmonicE = otherE = 0;
        for (int b = 20; b < N / 2; ++b)
        {
            const float hz = (float) b * binHz; const float h = hz / f0; const float d = std::fabs (h - std::round (h)) * f0 / binHz;
            if (d < 8.0f) harmonicE += (double) mag2[(size_t) b] * mag2[(size_t) b]; else otherE += (double) mag2[(size_t) b] * mag2[(size_t) b];
        }
        const float ratio2 = (float) (10.0 * std::log10 (otherE / std::max (harmonicE, 1.0e-20)));
        std::printf ("  VA saw non-harmonic / harmonic: %.1f dB\n", ratio2);
        CHECK (ratio2 < -70.0f, "virtual-analogue saw at C7 is alias-free (< -70 dB)");
    }

    // ---------------------------------------------------------------- 3. wavetable content sanity
    {
        std::printf ("[wavetables]\n");
        WavetableBank bank; bank.build (kSr);
        bool ok = true;
        for (int t = 0; t < WavetableBank::kNumTables; ++t)
        {
            const auto& wt = bank.get (t);
            for (int fr = 0; fr < wt.numFrames; ++fr)
            {
                const float* d = wt.ptr (fr, 0);
                float pk = 0; for (int i = 0; i < Wavetable::kFrameSize; ++i) { if (! std::isfinite (d[i])) ok = false; pk = std::max (pk, std::fabs (d[i])); }
                if (pk < 0.5f || pk > 1.001f) { ok = false; std::printf ("  table %s frame %d peak %.3f\n", wt.name.c_str(), fr, pk); }
            }
        }
        CHECK (ok, "all wavetable frames normalised and finite");
        CHECK (Wavetable::maxHarmonicForMip (0, kSr) > 500 && Wavetable::maxHarmonicForMip (9, kSr) == 1, "mip harmonic limits sensible");
    }

    // ---------------------------------------------------------------- 4. filter stability
    {
        std::printf ("[filters]\n");
        for (int t = 0; t < (int) FilterType::NumTypes; ++t)
        {
            SynthEngine e; e.prepare (kSr, 256);
            SynthParams p = cleanParams();
            p.filter[0].enabled = true; p.filter[0].type = (FilterType) t;
            p.filter[0].cutoff = 60.0f; p.filter[0].resonance = 1.0f; p.filter[0].drive = 1.0f;
            p.filter[0].envAmount = 1.0f; p.env[1].attack = 0.001f; p.env[1].decay = 0.3f; p.env[1].sustain = 0.0f;
            p.mod[0] = { ModSource::Lfo1, ModDest::Filter1Cutoff, 1.0f }; p.lfo[0].rateHz = 30.0f; p.lfo[0].shape = LfoShape::Square;
            e.setParams (p);
            std::vector<float> L, R; renderNote (e, 36, 1.5f, 1.0f, L, R);
            char msg[128]; std::snprintf (msg, sizeof (msg), "filter type %d stable under extreme modulation (peak %.2f)", t, peakOf (L));
            CHECK (allFinite (L) && peakOf (L) < 4.0f, msg);
        }
        // Frequency response sanity: LP24 at 1 kHz must attenuate 8 kHz strongly
        {
            Filter f; f.prepare (kSr); f.resetTo (1000.0f, 0.0f, 0.0f); f.setTargets (1000.0f, 0.0f, 0.0f);
            std::vector<float> in ((size_t) kSr), out ((size_t) kSr);
            for (size_t i = 0; i < in.size(); ++i) in[i] = std::sin (kTwoPi * 8000.0f * (float) i / kSr);
            for (size_t i = 0; i < in.size(); ++i) out[i] = f.process (in[i], FilterType::LP24);
            const float att = gainToDb (rmsOf (out, 24000) / rmsOf (in, 24000));
            std::printf ("  LP24 @1k attenuates 8k by %.1f dB\n", att);
            CHECK (att < -60.0f, "LP24 slope is 24 dB/oct class");
            Filter g; g.prepare (kSr); g.resetTo (1000.0f, 0.0f, 0.0f); g.setTargets (1000.0f, 0.0f, 0.0f);
            for (size_t i = 0; i < in.size(); ++i) in[i] = std::sin (kTwoPi * 100.0f * (float) i / kSr);
            for (size_t i = 0; i < in.size(); ++i) out[i] = g.process (in[i], FilterType::Ladder);
            const float pass = gainToDb (rmsOf (out, 24000) / rmsOf (in, 24000));
            std::printf ("  Ladder passband gain @100 Hz: %.2f dB\n", pass);
            CHECK (pass > -2.0f && pass < 1.0f, "ladder passband is flat");
        }
    }

    // ---------------------------------------------------------------- 5. half-band oversampler reconstruction
    {
        std::printf ("[oversampling]\n");
        HalfBand2x hb;
        std::vector<float> in ((size_t) 4000), out ((size_t) 4000);
        for (size_t i = 0; i < in.size(); ++i) in[i] = std::sin (kTwoPi * 3000.0f * (float) i / kSr);
        for (size_t i = 0; i < in.size(); ++i) { float a, b; hb.up (in[i], a, b); out[i] = hb.down (a, b); }
        const float g = rmsOf (out, 2000) / rmsOf (in, 2000);
        std::printf ("  up/down gain at 3 kHz: %.4f\n", g);
        CHECK (std::fabs (g - 1.0f) < 0.02f, "half-band up/down reconstructs unity gain");
        // image rejection: upsampled sine must not contain energy at fs - f
        std::vector<float> up; up.reserve (in.size() * 2);
        HalfBand2x hb2;
        for (size_t i = 0; i < in.size(); ++i) { float a, b; hb2.up (in[i], a, b); up.push_back (a); up.push_back (b); }
        auto mag = spectrum (std::vector<float> (up.begin() + 1000, up.begin() + 1000 + 4096), 4096);
        const float binHz2 = 2.0f * kSr / 4096.0f;
        const int bSig = (int) std::round (3000.0f / binHz2), bImg = (int) std::round ((kSr - 3000.0f) / binHz2);
        float sig = 0, img = 0; for (int k = -2; k <= 2; ++k) { sig = std::max (sig, mag[(size_t) (bSig + k)]); img = std::max (img, mag[(size_t) (bImg + k)]); }
        const float rej = gainToDb (img / sig);
        std::printf ("  image rejection: %.1f dB\n", rej);
        CHECK (rej < -80.0f, "half-band image rejection > 80 dB");
    }

    // ---------------------------------------------------------------- 6. envelope timing
    {
        std::printf ("[envelope]\n");
        Envelope env; env.prepare (kSr);
        EnvParams p; p.attack = 0.1f; p.decay = 0.2f; p.sustain = 0.5f; p.release = 0.1f; p.curve = 0.5f; p.velocity = 0.0f;
        env.noteOn (p, 1.0f);
        int reach = -1; std::vector<float> lv;
        for (int i = 0; i < (int) (kSr * 0.5f); ++i) { const float v = env.process(); lv.push_back (v); if (reach < 0 && v >= 0.999f) reach = i; }
        std::printf ("  attack reaches 1.0 after %.1f ms\n", reach / kSr * 1000.0f);
        CHECK (reach > 0 && std::fabs (reach / kSr - 0.1f) < 0.02f, "attack time within 20 ms of 100 ms");
        CHECK (std::fabs (lv.back() - 0.5f) < 0.01f, "sustain level reached");
        env.noteOff();
        int rel = -1; for (int i = 0; i < (int) kSr; ++i) { if (env.process() <= 0.0f) { rel = i; break; } }
        std::printf ("  release finishes after %.1f ms\n", rel / kSr * 1000.0f);
        CHECK (rel > 0 && rel / kSr < 0.2f, "release finishes within 200 ms");
    }

    // ---------------------------------------------------------------- 7. voice allocation & modes
    {
        std::printf ("[voices]\n");
        SynthEngine e; e.prepare (kSr, 256);
        SynthParams p = cleanParams(); p.voices = 8; p.env[0].release = 0.5f; e.setParams (p);
        std::vector<float> L (256), R (256);
        for (int n = 0; n < 40; ++n) { e.noteOn (40 + n, 0.8f); e.render (L.data(), R.data(), 256); }
        CHECK (e.getActiveVoiceCount() <= 8, "polyphony limit respected via voice stealing");
        e.panic();
        p.voiceMode = VoiceMode::Legato; p.glideTime = 0.05f; e.setParams (p);
        e.noteOn (60, 0.8f); e.render (L.data(), R.data(), 256); e.noteOn (67, 0.8f); e.render (L.data(), R.data(), 256);
        CHECK (e.getActiveVoiceCount() == 1, "legato mode uses one voice");
        e.noteOff (67); e.render (L.data(), R.data(), 256);
        CHECK (e.getActiveVoiceCount() == 1 && e.newestVoice()->getNote() == 60, "releasing top key returns to held key");
        e.noteOff (60); for (int i = 0; i < 200; ++i) e.render (L.data(), R.data(), 256);
        CHECK (e.getActiveVoiceCount() == 0, "mono voice ends after all keys released");
        // sustain pedal
        p.voiceMode = VoiceMode::Poly; p.env[0].release = 0.01f; e.setParams (p);
        e.setSustain (true); e.noteOn (60, 0.8f); e.noteOff (60); for (int i = 0; i < 20; ++i) e.render (L.data(), R.data(), 256);
        CHECK (e.getActiveVoiceCount() == 1, "sustain pedal holds note");
        e.setSustain (false); for (int i = 0; i < 200; ++i) e.render (L.data(), R.data(), 256);
        CHECK (e.getActiveVoiceCount() == 0, "pedal release ends note");
    }

    // ---------------------------------------------------------------- 8. effects
    {
        std::printf ("[effects]\n");
        SynthEngine e; e.prepare (kSr, 256);
        SynthParams p = cleanParams();
        p.fx.reverbOn = true; p.fx.reverbMix = 0.5f; p.fx.reverbDecay = 0.5f; p.fx.delayOn = true; p.fx.delayMix = 0.4f; p.fx.delaySync = false; p.fx.delayTimeMs = 100.0f;
        p.fx.chorusOn = true; p.fx.satOn = true; p.fx.satDrive = 0.5f; p.fx.eqOn = true; p.fx.eqHighGain = 4.0f; p.fx.compOn = true; p.fx.limiterOn = true;
        e.setParams (p);
        std::vector<float> L, R; renderNote (e, 60, 3.0f, 0.3f, L, R);
        CHECK (allFinite (L) && allFinite (R), "full effects chain is finite");
        CHECK (peakOf (L) <= dbToGain (-0.3f) + 1.0e-3f, "limiter holds ceiling at -0.3 dBFS");
        const float tail1 = rmsOf (L, 48000, 60000), tail2 = rmsOf (L, 120000, 132000);
        std::printf ("  reverb tail 1s: %.4f  2.5s: %.4f\n", tail1, tail2);
        CHECK (tail1 > 1.0e-4f && tail2 < tail1, "reverb/delay tail present and decaying");
        CHECK (std::fabs (rmsOf (L) - rmsOf (R)) / std::max (rmsOf (L), 1.0e-6f) < 0.5f, "stereo balance sane");
    }

    // ---------------------------------------------------------------- 9. modulation matrix
    {
        std::printf ("[modulation]\n");
        SynthEngine e; e.prepare (kSr, 256);
        SynthParams p = cleanParams();
        p.osc[0].engine = OscEngine::Analog; p.osc[0].shape = AnalogShape::Sine;
        p.mod[0] = { ModSource::Macro1, ModDest::PitchAll, 0.5f }; // +12 semitones at macro 1
        e.setParams (p);
        std::vector<float> L, R; renderNote (e, 69, 0.5f, 0.5f, L, R);
        auto m1 = spectrum (std::vector<float> (L.begin() + 4000, L.begin() + 4000 + 16384), 16384);
        int b1 = 0; for (size_t i = 1; i < m1.size(); ++i) if (m1[i] > m1[(size_t) b1]) b1 = (int) i;
        p.macros[0] = 1.0f; e.setParams (p);
        renderNote (e, 69, 0.5f, 0.5f, L, R);
        auto m2 = spectrum (std::vector<float> (L.begin() + 4000, L.begin() + 4000 + 16384), 16384);
        int b2 = 0; for (size_t i = 1; i < m2.size(); ++i) if (m2[i] > m2[(size_t) b2]) b2 = (int) i;
        std::printf ("  peak bins: %d -> %d (%.1f Hz -> %.1f Hz)\n", b1, b2, b1 * kSr / 16384.0f, b2 * kSr / 16384.0f);
        CHECK (std::abs (b2 - 2 * b1) <= 2, "macro -> pitch modulation shifts one octave");
    }

    // ---------------------------------------------------------------- 10. demo render
    {
        std::printf ("[demo]\n");
        SynthEngine e; e.prepare (kSr, 256);
        SynthParams p;
        p.osc[0].table = 1; p.osc[0].unison = 5; p.osc[0].detune = 0.35f; p.osc[0].spread = 0.8f;
        p.osc[1].enabled = true; p.osc[1].table = 2; p.osc[1].octave = -1; p.osc[1].level = 0.5f; p.osc[1].unison = 3;
        p.subLevel = 0.3f;
        p.filter[0].type = FilterType::Ladder; p.filter[0].cutoff = 900.0f; p.filter[0].resonance = 0.35f; p.filter[0].envAmount = 0.6f; p.filter[0].drive = 0.2f;
        p.env[1].attack = 0.01f; p.env[1].decay = 0.6f; p.env[1].sustain = 0.2f;
        p.env[0].attack = 0.01f; p.env[0].release = 0.8f;
        p.lfo[0].rateHz = 0.3f; p.mod[0] = { ModSource::Lfo1, ModDest::OscAMorph, 0.4f };
        p.fx.chorusOn = true; p.fx.delayOn = true; p.fx.delayMix = 0.25f; p.fx.reverbOn = true; p.fx.reverbMix = 0.3f; p.fx.reverbDecay = 0.6f;
        e.setParams (p);
        const int notes[8] = { 48, 55, 60, 63, 67, 70, 72, 75 };
        const int total = (int) (kSr * 6.0f);
        std::vector<float> L ((size_t) total, 0.0f), R ((size_t) total, 0.0f);
        int pos = 0; const int block = 256;
        while (pos < total)
        {
            const int step = (int) (kSr * 0.5f);
            const int idx = pos / step;
            if (pos % step < block && idx < 8) { e.noteOn (notes[idx], 0.8f); }
            if (pos % step >= step - block && pos % step < step && idx < 8) e.noteOff (notes[idx]);
            e.render (L.data() + pos, R.data() + pos, std::min (block, total - pos));
            pos += block;
        }
        CHECK (allFinite (L) && peakOf (L) <= 1.0f, "demo sequence renders cleanly");
        writeWav ("lucid_demo.wav", L, R);
        std::printf ("  wrote lucid_demo.wav (peak %.2f dBFS)\n", gainToDb (peakOf (L)));
    }

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
