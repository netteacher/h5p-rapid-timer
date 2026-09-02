// LUCID PULSE - polyrhythmic step sequencer core (no JUCE dependency).
// 8 lanes, each with its own step count and clock division (polymeter) or with N steps stretched
// over 1/2/4 bars (polyrhythm). Sample-accurate event generation from a beat position, with swing,
// nudge, humanize, probability, ratchets and Euclidean patterns.
#pragma once
#include "../../Source/dsp/Core.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>

namespace pulse {

constexpr int kNumLanes = 8;
constexpr int kMaxSteps = 32;
constexpr int kNumPatterns = 4;
constexpr int kNumDivisions = 7;

enum class StepState : int { Off = 0, On = 1, Accent = 2, Ghost = 3, Maybe = 4 };
enum class LaneMode : int { Polymeter = 0, Polyrhythm = 1 };

// Clock divisions (in beats)
inline float divisionBeats (int idx)
{
    static const float d[kNumDivisions] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f, 1.0f / 12.0f };
    return d[idx < 0 ? 0 : (idx >= kNumDivisions ? kNumDivisions - 1 : idx)];
}
inline const char* divisionName (int idx)
{
    static const char* n[kNumDivisions] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T" };
    return n[idx < 0 ? 0 : (idx >= kNumDivisions ? kNumDivisions - 1 : idx)];
}

struct LaneParams
{
    bool  enabled = true;
    int   steps = 16;                 // 1..32
    int   division = 3;               // index into divisionBeats (polymeter)
    LaneMode mode = LaneMode::Polymeter;
    int   spanBars = 1;               // polyrhythm: steps are spread over 1/2/4 bars
    bool  euclid = false;             // pattern generated from hits/rotation instead of the grid
    int   hits = 4, rotation = 0;
    float probability = 1.0f;         // 0..1 (applies to every step; "Maybe" steps use half of it)
    float swing = 0.0f;               // 0..1 lane swing (added to global)
    float nudgeMs = 0.0f;             // -30..30
    float ratchet = 0.0f;             // 0..1 probability of a 2-3 hit ratchet per step
    float gate = 0.5f;                // 0.05..1 fraction of the step
    float velocity = 1.0f;            // 0..1
    float accent = 0.3f;              // 0..1 extra velocity for accented steps
    int   midiNote = 36;
    int   midiChannel = 10;
};

struct GlobalParams
{
    float swing = 0.0f;               // 0..1  (1 = MPC 75 %)
    float humanizeTime = 0.0f;        // 0..1  (up to +-12 ms)
    float humanizeVel = 0.0f;         // 0..1
    float density = 0.5f;             // 0..1  (0.5 = neutral)
    bool  fill = false;
    int   beatsPerBar = 4;
};

struct Pattern
{
    std::array<std::array<uint8_t, kMaxSteps>, kNumLanes> steps {};
    void clear() { for (auto& l : steps) l.fill (0); }
};

struct SeqEvent
{
    int lane; int sampleOffset; float velocity; bool noteOn; int note; int channel; int durationSamples;
};

// Bjorklund / Euclidean rhythm
inline std::array<uint8_t, kMaxSteps> euclidean (int steps, int hits, int rotation)
{
    std::array<uint8_t, kMaxSteps> out {};
    steps = std::max (1, std::min (kMaxSteps, steps));
    hits = std::max (0, std::min (steps, hits));
    for (int i = 0; i < steps; ++i)
    {
        // Bresenham formulation: hit when the accumulated fraction wraps
        const int idx = ((i - rotation) % steps + steps) % steps;
        const bool hit = hits > 0 && ((idx * hits) % steps) < hits;
        out[(size_t) i] = hit ? 1 : 0;
    }
    return out;
}

class Sequencer
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        rng = lucid::Random (0xC0FFEEu);
        for (auto& l : lanes) l = LaneState();
        internalBeat = 0.0;
    }

    void setPattern (const Pattern& p) { pattern = p; }
    Pattern& getPattern() { return pattern; }
    const Pattern& getPattern() const { return pattern; }
    void setLaneParams (int lane, const LaneParams& p) { laneParams[(size_t) lane] = p; }
    void setGlobal (const GlobalParams& g) { global = g; }

    // Effective step pattern for a lane (grid or Euclidean)
    std::array<uint8_t, kMaxSteps> effectiveSteps (int lane) const
    {
        const auto& lp = laneParams[(size_t) lane];
        if (lp.euclid) return euclidean (lp.steps, lp.hits, lp.rotation);
        return pattern.steps[(size_t) lane];
    }

    float stepLengthBeats (int lane) const
    {
        const auto& lp = laneParams[(size_t) lane];
        if (lp.mode == LaneMode::Polyrhythm)
            return (float) (lp.spanBars * global.beatsPerBar) / (float) std::max (1, lp.steps);
        return divisionBeats (lp.division);
    }

    // Current step index for display, given a beat position
    int currentStep (int lane, double beat) const
    {
        const auto& lp = laneParams[(size_t) lane];
        const double len = stepLengthBeats (lane);
        const long long n = (long long) std::floor (beat / len);
        return (int) (((n % lp.steps) + lp.steps) % lp.steps);
    }

    // Generates the events for a block. If 'hostPlaying' the given beat position is used, else the
    // internal clock advances. Returns the beat position at the start of the block.
    double process (bool hostPlaying, double hostBeat, double bpm, int numSamples, std::vector<SeqEvent>& events)
    {
        events.clear();
        const double samplesPerBeat = sr * 60.0 / std::max (20.0, bpm);
        const double beatsInBlock = (double) numSamples / samplesPerBeat;
        double start;
        if (hostPlaying) { start = hostBeat; internalBeat = hostBeat + beatsInBlock; }
        else { start = internalBeat; internalBeat += beatsInBlock; }
        const double end = start + beatsInBlock;
        if (! running) return start;

        for (int l = 0; l < kNumLanes; ++l)
        {
            auto& st = lanes[(size_t) l];
            const auto& lp = laneParams[(size_t) l];

            // pending note-offs
            if (st.noteOffCountdown > 0)
            {
                if (st.noteOffCountdown <= numSamples)
                {
                    events.push_back ({ l, st.noteOffCountdown - 1, 0.0f, false, st.playingNote, lp.midiChannel, 0 });
                    st.noteOffCountdown = 0;
                }
                else st.noteOffCountdown -= numSamples;
            }
            if (! lp.enabled) continue;

            const double len = stepLengthBeats (l);
            const auto steps = effectiveSteps (l);
            const double swingTotal = lucid::clampf (global.swing + lp.swing, 0.0f, 1.0f);
            const double maxOffsetBeats = 0.5 * len + 0.05 * samplesPerBeat / samplesPerBeat + (30.0 + 12.0) / 1000.0 * bpm / 60.0;
            const long long nFirst = (long long) std::floor ((start - maxOffsetBeats) / len) - 1;
            const long long nLast  = (long long) std::ceil ((end + maxOffsetBeats) / len) + 1;

            for (long long n = std::max (0LL, nFirst); n <= nLast; ++n)
            {
                const int idx = (int) (n % lp.steps);
                if (idx >= lp.steps) continue;
                const auto state = (StepState) steps[(size_t) idx];
                if (state == StepState::Off) continue;

                // timing: swing on odd steps, nudge, humanize (deterministic per step index & cycle)
                double t = (double) n * len;
                if ((n & 1) && lp.mode == LaneMode::Polymeter) t += swingTotal * 0.5 * len;
                t += (double) lp.nudgeMs / 1000.0 * bpm / 60.0;
                uint32_t h = (uint32_t) (n * 2654435761ull) ^ (uint32_t) (l * 40503u) ^ seed; // hash -> well mixed per-step randomness
                h ^= h >> 16; h *= 0x7feb352dU; h ^= h >> 15; h *= 0x846ca68bU; h ^= h >> 16;
                lucid::Random stepRng (h); stepRng.next(); stepRng.next();
                if (global.humanizeTime > 0.0f) t += stepRng.bipolar() * global.humanizeTime * 0.012 * bpm / 60.0;
                if (t < start || t >= end) continue;

                // probability & density
                float prob = lp.probability;
                if (state == StepState::Maybe) prob *= 0.5f;
                const float densityBias = (global.density - 0.5f) * 2.0f; // -1..1
                prob = lucid::clampf (prob + densityBias * 0.5f, 0.0f, 1.0f);
                if (global.fill) prob = 1.0f;
                if (prob < 1.0f && stepRng.uniform() > prob) continue;

                // velocity
                float vel = lp.velocity;
                if (state == StepState::Accent) vel = lucid::clampf (vel + lp.accent, 0.0f, 1.0f);
                else if (state == StepState::Ghost) vel *= 0.45f;
                if (global.humanizeVel > 0.0f) vel = lucid::clampf (vel + stepRng.bipolar() * global.humanizeVel * 0.25f, 0.05f, 1.0f);

                // ratchet: 2 or 3 hits inside the step
                int hits = 1;
                const float ratchetProb = global.fill ? std::max (lp.ratchet, 0.35f) : lp.ratchet;
                if (ratchetProb > 0.0f && stepRng.uniform() < ratchetProb) hits = stepRng.uniform() < 0.35f ? 3 : 2;
                const double sub = len / (double) hits;
                for (int h = 0; h < hits; ++h)
                {
                    const double th = t + h * sub;
                    if (th >= end) break; // (rest lands in a later block via nothing - acceptable for ratchets near the block end)
                    int offset = (int) std::floor ((th - start) * samplesPerBeat);
                    offset = std::max (0, std::min (numSamples - 1, offset));
                    const int gateSamples = std::max (1, (int) (lp.gate * sub * samplesPerBeat));
                    // choke the previous note of this lane
                    if (st.noteOffCountdown > 0)
                    {
                        events.push_back ({ l, offset, 0.0f, false, st.playingNote, lp.midiChannel, 0 });
                        st.noteOffCountdown = 0;
                    }
                    const float v = h == 0 ? vel : vel * (0.7f + 0.1f * (float) h);
                    events.push_back ({ l, offset, v, true, lp.midiNote, lp.midiChannel, gateSamples });
                    st.playingNote = lp.midiNote;
                    const int remaining = numSamples - offset;
                    if (gateSamples < remaining) events.push_back ({ l, offset + gateSamples, 0.0f, false, lp.midiNote, lp.midiChannel, 0 });
                    else st.noteOffCountdown = gateSamples - remaining;
                }
            }
        }
        // keep note-offs after note-ons at the same offset ordering sane
        std::stable_sort (events.begin(), events.end(), [] (const SeqEvent& a, const SeqEvent& b) { return a.sampleOffset < b.sampleOffset; });
        return start;
    }

    void setRunning (bool r) { running = r; if (! r) for (auto& l : lanes) l.noteOffCountdown = 0; }
    bool isRunning() const { return running; }
    void resetClock() { internalBeat = 0.0; }
    void reseed() { seed = rng.next(); }
    double getInternalBeat() const { return internalBeat; }
    const LaneParams& getLaneParams (int l) const { return laneParams[(size_t) l]; }
    const GlobalParams& getGlobal() const { return global; }

private:
    struct LaneState { int noteOffCountdown = 0; int playingNote = 36; };
    float sr = 48000.0f;
    Pattern pattern;
    std::array<LaneParams, kNumLanes> laneParams;
    std::array<LaneState, kNumLanes> lanes;
    GlobalParams global;
    lucid::Random rng;
    uint32_t seed = 12345u;
    double internalBeat = 0.0;
    bool running = true;
};

// ---------------------------------------------------------------------------- style generator
// Produces musically sensible starting points for a whole kit (8 lanes: kick, snare, clap, closed
// hat, open hat, perc, rim, noise).
struct GeneratedLane { std::array<uint8_t, kMaxSteps> steps {}; int numSteps = 16; int division = 3; LaneMode mode = LaneMode::Polymeter; int spanBars = 1; float probability = 1.0f; float ratchet = 0.0f; };

inline void generateStyle (int style, uint32_t seed, std::array<GeneratedLane, kNumLanes>& out)
{
    lucid::Random r (seed | 1u);
    auto put = [] (GeneratedLane& g, std::initializer_list<int> ons, int accentEvery = 0)
    {
        g.steps.fill (0);
        for (int i : ons) if (i < kMaxSteps) g.steps[(size_t) i] = (accentEvery > 0 && i % accentEvery == 0) ? 2 : 1;
    };
    auto euclidLane = [] (GeneratedLane& g, int steps, int hits, int rot, int div = 3)
    {
        g.numSteps = steps; g.division = div; g.steps = euclidean (steps, hits, rot);
    };
    for (auto& g : out) g = GeneratedLane();
    auto& kick = out[0]; auto& snare = out[1]; auto& clap = out[2]; auto& ch = out[3]; auto& oh = out[4]; auto& perc = out[5]; auto& rim = out[6]; auto& noise = out[7];

    switch (style)
    {
        case 0: // Minimal Techno
            put (kick, { 0, 4, 8, 12 }, 8);
            put (clap, { 4, 12 });
            snare.steps.fill (0); snare.steps[14] = 4; snare.probability = 0.6f;
            put (ch, { 2, 6, 10, 14 }); ch.steps[(size_t) (r.next() % 16)] = 3;
            oh.steps.fill (0); oh.steps[(size_t) (8 + (r.next() % 2) * 4 + 2)] = 1; oh.probability = 0.8f;
            euclidLane (perc, 12, 5, (int) (r.next() % 12)); perc.probability = 0.8f;
            euclidLane (rim, 7, 3, (int) (r.next() % 7)); rim.probability = 0.7f;
            noise.steps.fill (0); noise.steps[15] = 4; noise.numSteps = 32; noise.steps[31] = 1; noise.ratchet = 0.5f;
            break;
        case 1: // Deep House
            put (kick, { 0, 4, 8, 12 }, 16);
            put (clap, { 4, 12 }); clap.steps[13] = 3;
            put (snare, { 12 }); snare.probability = 0.5f;
            put (ch, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }); for (int i = 0; i < 16; i += 2) ch.steps[(size_t) i] = 3; ch.steps[(size_t) (r.next() % 16)] = 0;
            put (oh, { 2, 6, 10, 14 });
            euclidLane (perc, 16, 6, 3); perc.probability = 0.75f;
            euclidLane (rim, 5, 2, (int) (r.next() % 5)); rim.probability = 0.85f;
            noise.steps.fill (0); noise.numSteps = 8; noise.division = 1; noise.steps[3] = 1; noise.steps[7] = 3;
            break;
        case 2: // Dub Techno
            put (kick, { 0, 4, 8, 12 }, 8); kick.steps[11] = 3;
            put (clap, { 4, 12 }); clap.probability = 0.9f;
            put (snare, { 15 }); snare.steps[15] = 4;
            put (ch, { 2, 6, 10, 14 }); ch.steps[7] = 3;
            oh.steps.fill (0); oh.steps[10] = 1; oh.probability = 0.6f;
            euclidLane (perc, 9, 4, (int) (r.next() % 9)); perc.mode = LaneMode::Polyrhythm; perc.spanBars = 2; perc.probability = 0.7f;
            euclidLane (rim, 11, 5, (int) (r.next() % 11)); rim.probability = 0.5f;
            noise.steps.fill (0); noise.numSteps = 32; noise.steps[0] = 1; noise.steps[24] = 4;
            break;
        case 3: // Polyrhythm 5:4 / 7:8
            put (kick, { 0, 4, 8, 12 }, 16);
            euclidLane (clap, 5, 2, 1); clap.mode = LaneMode::Polyrhythm; clap.spanBars = 1;
            euclidLane (snare, 7, 3, 0); snare.mode = LaneMode::Polyrhythm; snare.spanBars = 2; snare.probability = 0.8f;
            euclidLane (ch, 5, 5, 0); ch.mode = LaneMode::Polyrhythm; ch.spanBars = 1; for (int i = 0; i < 5; ++i) ch.steps[(size_t) i] = i == 0 ? 2 : 3;
            put (oh, { 6, 14 }); oh.probability = 0.7f;
            euclidLane (perc, 7, 4, 2); perc.mode = LaneMode::Polyrhythm; perc.spanBars = 1;
            euclidLane (rim, 3, 2, 0); rim.mode = LaneMode::Polyrhythm; rim.spanBars = 1;
            euclidLane (noise, 9, 2, 4); noise.mode = LaneMode::Polyrhythm; noise.spanBars = 4; noise.probability = 0.8f;
            break;
        case 4: // Broken / UK
            put (kick, { 0, 6, 10 }, 16); kick.steps[13] = 3;
            put (clap, { 4, 12 });
            put (snare, { 12, 15 }); snare.steps[15] = 3;
            put (ch, { 0, 2, 3, 4, 6, 8, 10, 11, 12, 14 }); ch.steps[3] = 3; ch.steps[11] = 3;
            put (oh, { 9 }); oh.probability = 0.8f;
            euclidLane (perc, 16, 7, 5); perc.probability = 0.6f; perc.ratchet = 0.15f;
            euclidLane (rim, 13, 5, 2); rim.probability = 0.6f;
            noise.steps.fill (0); noise.numSteps = 16; noise.steps[7] = 4; noise.steps[15] = 1; noise.ratchet = 0.3f;
            break;
        default: // Afro / 12-8 feel (triplet grid)
            put (kick, { 0, 3, 6, 9 }, 6); kick.numSteps = 12; kick.division = 4; kick.steps[8] = 3;
            euclidLane (clap, 12, 4, 3, 4);
            euclidLane (snare, 12, 5, 1, 4); snare.probability = 0.6f;
            euclidLane (ch, 12, 12, 0, 4); for (int i = 0; i < 12; ++i) ch.steps[(size_t) i] = (i % 3 == 0) ? 2 : 3;
            euclidLane (oh, 12, 2, 5, 4);
            euclidLane (perc, 12, 7, 2, 4);
            euclidLane (rim, 12, 5, 4, 4); rim.probability = 0.8f;
            euclidLane (noise, 12, 3, 1, 4); noise.probability = 0.5f;
            break;
    }
}

inline const char* styleName (int idx)
{
    static const char* n[6] = { "Minimal Techno", "Deep House", "Dub Techno", "Polyrhythm 5:4 / 7:8", "Broken / UK", "Afro 12/8" };
    return n[idx < 0 ? 0 : (idx > 5 ? 5 : idx)];
}
constexpr int kNumStyles = 6;

} // namespace pulse
