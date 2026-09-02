// LUCID PULSE - offline tests for the sequencer core and drum voices
#include "../Pulse/dsp/Sequencer.h"
#include "../Pulse/dsp/DrumKit.h"
#include "../Source/dsp/Effects.h"
#include <cstdio>
#include <fstream>
#include <map>

using namespace pulse;
static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { ++checks; if (!(cond)) { ++failures; std::printf ("  FAIL: %s\n", msg); } else std::printf ("  ok:   %s\n", msg); } while (0)

static const float kSr = 48000.0f;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    std::ofstream f (path, std::ios::binary); if (! f) return;
    const uint32_t n = (uint32_t) L.size(); const uint32_t dataBytes = n * 4; const uint32_t sr = (uint32_t) kSr;
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); }; auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVE", 4); f.write ("fmt ", 4); w32 (16); w16 (1); w16 (2); w32 (sr); w32 (sr * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i) { w16 ((uint16_t) (int16_t) (lucid::clampf (L[i], -1, 1) * 32767)); w16 ((uint16_t) (int16_t) (lucid::clampf (R[i], -1, 1) * 32767)); }
}

int main()
{
    std::printf ("LUCID PULSE tests\n");
    // ---------------------------------------------------------------- Euclid
    {
        std::printf ("[euclid]\n");
        auto e = euclidean (8, 3, 0); int hits = 0; for (int i = 0; i < 8; ++i) hits += e[(size_t) i];
        CHECK (hits == 3, "E(3,8) has 3 hits");
        auto e2 = euclidean (16, 16, 0); hits = 0; for (int i = 0; i < 16; ++i) hits += e2[(size_t) i];
        CHECK (hits == 16, "E(16,16) is full");
        auto e3 = euclidean (12, 5, 0); std::string s; for (int i = 0; i < 12; ++i) s += e3[(size_t) i] ? 'x' : '.';
        std::printf ("  E(5,12) = %s\n", s.c_str());
        int maxGap = 0, gap = 0; for (int i = 0; i < 24; ++i) { if (e3[(size_t) (i % 12)]) { maxGap = std::max (maxGap, gap); gap = 0; } else ++gap; }
        CHECK (maxGap <= 2, "E(5,12) hits are evenly spread");
        auto r0 = euclidean (8, 3, 0), r1 = euclidean (8, 3, 1);
        bool rotated = true; for (int i = 0; i < 8; ++i) if (r1[(size_t) i] != r0[(size_t) ((i - 1 + 8) % 8)]) rotated = false;
        CHECK (rotated, "rotation shifts the pattern by one step");
    }
    // ---------------------------------------------------------------- timing
    {
        std::printf ("[timing]\n");
        Sequencer seq; seq.prepare (kSr);
        Pattern p; p.clear();
        for (int i = 0; i < 16; i += 4) p.steps[0][(size_t) i] = 1; // four-on-the-floor on lane 0 (1/16 grid)
        for (int i = 0; i < 16; i += 2) p.steps[1][(size_t) i] = 1;   // lane 1: every 8th on a 1/16 grid
        seq.setPattern (p);
        LaneParams lp; seq.setLaneParams (0, lp); seq.setLaneParams (1, lp);
        for (int l = 2; l < kNumLanes; ++l) { LaneParams off; off.enabled = false; seq.setLaneParams (l, off); }
        GlobalParams g; seq.setGlobal (g);
        std::vector<SeqEvent> ev;
        const double bpm = 120.0; const int block = 256;
        std::vector<double> kickTimes; int lane1Ons = 0;
        double beat = 0.0; const double samplesPerBeat = kSr * 60.0 / bpm;
        for (int b = 0; b < (int) (samplesPerBeat * 8.0 / block); ++b)
        {
            seq.process (true, beat, bpm, block, ev);
            for (const auto& e : ev)
            {
                if (! e.noteOn) continue;
                const double t = beat + e.sampleOffset / samplesPerBeat;
                if (e.lane == 0) kickTimes.push_back (t); else if (e.lane == 1) ++lane1Ons;
            }
            beat += block / samplesPerBeat;
        }
        std::printf ("  kicks in 8 beats: %d, lane1 hits: %d\n", (int) kickTimes.size(), lane1Ons);
        CHECK (kickTimes.size() == 8, "four-on-the-floor gives one kick per beat over 8 beats");
        CHECK (lane1Ons == 16, "1/8 lane gives 16 hits over 8 beats");
        double maxErr = 0; for (size_t i = 0; i < kickTimes.size(); ++i) maxErr = std::max (maxErr, std::fabs (kickTimes[i] - (double) i));
        std::printf ("  max kick timing error: %.3f ms\n", maxErr * 60.0 / bpm * 1000.0);
        CHECK (maxErr * 60.0 / bpm * 1000.0 < 0.05, "kick timing is sample accurate");
        // swing: odd 16ths delayed
        p.clear(); for (int i = 0; i < 16; ++i) p.steps[0][(size_t) i] = 1; seq.setPattern (p);
        g.swing = 1.0f; seq.setGlobal (g);
        std::vector<double> times; beat = 0.0;
        for (int b = 0; b < (int) (samplesPerBeat * 2.0 / block); ++b)
        {
            seq.process (true, beat, bpm, block, ev);
            for (const auto& e : ev) if (e.noteOn && e.lane == 0) times.push_back (beat + e.sampleOffset / samplesPerBeat);
            beat += block / samplesPerBeat;
        }
        CHECK (times.size() >= 4 && std::fabs (times[1] - 0.375) < 0.002 && std::fabs (times[2] - 0.5) < 0.002, "full swing puts odd 16ths at the triplet position (MPC 75%)");
        // polyrhythm: 5 steps over one bar -> 5 evenly spaced hits per 4 beats
        g.swing = 0.0f; seq.setGlobal (g);
        LaneParams poly; poly.mode = LaneMode::Polyrhythm; poly.steps = 5; poly.spanBars = 1; poly.euclid = true; poly.hits = 5;
        seq.setLaneParams (0, poly);
        times.clear(); beat = 0.0;
        for (int b = 0; b < (int) (samplesPerBeat * 4.0 / block) + 1; ++b)
        {
            seq.process (true, beat, bpm, block, ev);
            for (const auto& e : ev) if (e.noteOn && e.lane == 0 && beat + e.sampleOffset / samplesPerBeat < 3.99) times.push_back (beat + e.sampleOffset / samplesPerBeat);
            beat += block / samplesPerBeat;
        }
        bool spaced = times.size() == 5; for (size_t i = 0; spaced && i < times.size(); ++i) if (std::fabs (times[i] - i * 0.8) > 0.002) spaced = false;
        std::printf ("  polyrhythm 5 over 1 bar: %d hits\n", (int) times.size());
        CHECK (spaced, "5-step polyrhythm lane spreads 5 hits evenly over the bar");
        // probability 0 => silence; note-offs always follow note-ons
        LaneParams silent; silent.probability = 0.0f; seq.setLaneParams (0, silent);
        int ons = 0; for (int b = 0; b < 100; ++b) { seq.process (true, beat, bpm, block, ev); for (auto& e : ev) ons += e.noteOn; beat += block / samplesPerBeat; }
        CHECK (ons == 0, "probability 0 produces no notes");
        LaneParams normal; seq.setLaneParams (0, normal); p.clear(); for (int i = 0; i < 16; ++i) p.steps[0][(size_t) i] = 1; seq.setPattern (p);
        int onCount = 0, offCount = 0;
        for (int b = 0; b < 400; ++b) { seq.process (true, beat, bpm, block, ev); for (auto& e : ev) { if (e.noteOn) ++onCount; else ++offCount; } beat += block / samplesPerBeat; }
        std::printf ("  ons %d offs %d\n", onCount, offCount);
        CHECK (std::abs (onCount - offCount) <= 1, "every note-on gets a note-off");
        // free-running clock advances without host
        Sequencer free; free.prepare (kSr); free.setPattern (p); free.setLaneParams (0, normal);
        for (int l = 1; l < kNumLanes; ++l) { LaneParams off; off.enabled = false; free.setLaneParams (l, off); }
        int freeOns = 0; for (int b = 0; b < 600; ++b) { free.process (false, 0.0, bpm, block, ev); for (auto& e : ev) freeOns += e.noteOn; }
        CHECK (freeOns > 10 && free.getInternalBeat() > 4.0, "internal clock runs when the host is stopped");
    }
    // ---------------------------------------------------------------- style generator
    {
        std::printf ("[styles]\n");
        bool ok = true;
        for (int s = 0; s < kNumStyles; ++s)
        {
            std::array<GeneratedLane, kNumLanes> g; generateStyle (s, 42u + (uint32_t) s, g);
            int kicks = 0; for (int i = 0; i < g[0].numSteps; ++i) kicks += g[0].steps[(size_t) i] != 0;
            if (kicks < 3) { ok = false; std::printf ("  style %s has only %d kicks\n", styleName (s), kicks); }
            for (auto& l : g) if (l.numSteps < 1 || l.numSteps > kMaxSteps) ok = false;
        }
        CHECK (ok, "all styles generate valid lanes with a kick pattern");
    }
    // ---------------------------------------------------------------- drum voices + demo render
    {
        std::printf ("[drums]\n");
        DrumKit kit; kit.prepare (kSr);
        bool allFinite = true, allSound = true;
        for (int v = 0; v < 8; ++v)
        {
            DrumKit k2; k2.prepare (kSr);
            VoiceParams vp; k2.trigger (v, vp, 1.0f);
            double e = 0; float peak = 0;
            for (int i = 0; i < 24000; ++i) { float l, r; k2.process (l, r); if (! std::isfinite (l)) allFinite = false; e += l * l; peak = std::max (peak, std::fabs (l)); }
            std::printf ("  %-7s rms %.3f peak %.2f\n", voiceName (v), std::sqrt (e / 24000.0), peak);
            if (e < 1.0e-3 || peak > 1.5f) allSound = false;
        }
        CHECK (allFinite, "all drum voices are finite");
        CHECK (allSound, "all drum voices produce sound at sane levels");

        // full demo: Minimal Techno style through sequencer + kit + bus, 8 bars at 128 bpm
        Sequencer seq; seq.prepare (kSr);
        std::array<GeneratedLane, kNumLanes> g; generateStyle (0, 7u, g);
        Pattern p; for (int l = 0; l < kNumLanes; ++l) p.steps[(size_t) l] = g[(size_t) l].steps; seq.setPattern (p);
        for (int l = 0; l < kNumLanes; ++l) { LaneParams lp; lp.steps = g[(size_t) l].numSteps; lp.division = g[(size_t) l].division; lp.mode = g[(size_t) l].mode; lp.spanBars = g[(size_t) l].spanBars; lp.probability = g[(size_t) l].probability; lp.ratchet = g[(size_t) l].ratchet; seq.setLaneParams (l, lp); }
        GlobalParams gp; gp.swing = 0.15f; gp.humanizeVel = 0.2f; seq.setGlobal (gp);
        std::array<VoiceParams, 8> vps; vps[0].decay = 0.4f; vps[0].ctl[0] = 0.6f; vps[3].level = 0.5f; vps[3].decay = 0.15f; vps[4].level = 0.4f; vps[7].level = 0.35f; vps[5].tune = 7; vps[5].level = 0.5f; vps[6].level = 0.45f; vps[2].level = 0.6f; vps[1].level = 0.5f;
        lucid::EffectsChain bus; bus.prepare (kSr); lucid::SynthParams sp; sp.fx.compOn = true; sp.fx.compThreshold = -12.0f; sp.fx.compRatio = 3.0f; sp.fx.satOn = true; sp.fx.satDrive = 0.2f; sp.fx.limiterOn = true; sp.bpm = 128.0;
        const double bpm = 128.0; const int block = 256; const int total = (int) (kSr * 60.0 / bpm * 32.0);
        std::vector<float> L ((size_t) total, 0.0f), R ((size_t) total, 0.0f); std::vector<SeqEvent> ev;
        double beat = 0.0; const double spb = kSr * 60.0 / bpm; std::map<int, int> hitsPerLane;
        for (int pos = 0; pos + block <= total; pos += block)
        {
            seq.process (true, beat, bpm, block, ev);
            size_t ei = 0;
            for (int i = 0; i < block; ++i)
            {
                while (ei < ev.size() && ev[ei].sampleOffset <= i) { if (ev[ei].noteOn) { kit.trigger (ev[ei].lane, vps[(size_t) ev[ei].lane], ev[ei].velocity); ++hitsPerLane[ev[ei].lane]; } ++ei; }
                float l, r; kit.process (l, r); L[(size_t) (pos + i)] = l; R[(size_t) (pos + i)] = r;
            }
            bus.process (L.data() + pos, R.data() + pos, block, sp, lucid::FxModulation());
            beat += block / spb;
        }
        float peak = 0; bool fin = true; for (float x : L) { peak = std::max (peak, std::fabs (x)); if (! std::isfinite (x)) fin = false; }
        std::printf ("  hits: kick %d snare %d clap %d ch %d oh %d perc %d rim %d noise %d, peak %.2f\n", hitsPerLane[0], hitsPerLane[1], hitsPerLane[2], hitsPerLane[3], hitsPerLane[4], hitsPerLane[5], hitsPerLane[6], hitsPerLane[7], peak);
        CHECK (fin && peak <= 1.0f && hitsPerLane[0] == 32, "8-bar minimal techno demo renders: 32 kicks, finite, limited");
        writeWav ("pulse_demo.wav", L, R);
        std::printf ("  wrote pulse_demo.wav\n");
    }
    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
