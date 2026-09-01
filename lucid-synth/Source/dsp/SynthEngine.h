// LUCID Synth - polyphonic engine: voice allocation, mono/legato modes, global LFOs,
// global modulation for effects, and the effects chain.
#pragma once
#include "Core.h"
#include "Params.h"
#include "Voice.h"
#include "Effects.h"
#include <memory>

namespace lucid {

class SynthEngine
{
public:
    void prepare (float sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxBlock = std::max (16, maxBlockSize);
        bank.build (sr);
        for (int i = 0; i < kMaxVoices; ++i) voices[(size_t) i].prepare (sr, 1000u + (uint32_t) i * 7919u, &bank);
        for (int i = 0; i < kNumLfos; ++i) { globalLfos[(size_t) i].prepare (sr, 42u + (uint32_t) i); globalLfoBuf[(size_t) i].assign ((size_t) maxBlock, 0.0f); }
        fx.prepare (sr);
        pitchBendSmooth.prepare (sr, 5.0f); pitchBendSmooth.reset (0.0f);
        heldNotes.clear();
        stamp = 0;
        sustain = false;
    }

    void setParams (const SynthParams& p) { params = p; }
    const SynthParams& getParams() const { return params; }
    const WavetableBank& getWavetables() const { return bank; }

    // ---- MIDI ---------------------------------------------------------------------------
    void noteOn (int note, float velocity)
    {
        if (velocity <= 0.0f) { noteOff (note); return; }
        ++stamp;
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        heldNotes.push_back (note);
        const float glideFrom = lastNote >= 0 ? (float) lastNote : -1.0f;

        if (params.voiceMode == VoiceMode::Poly)
        {
            Voice* v = allocateVoice();
            v->noteOn (note, velocity, params, glideFrom, false, stamp);
        }
        else
        {
            Voice& v = voices[0];
            const bool legato = params.voiceMode == VoiceMode::Legato && v.isActive() && ! v.isReleasing();
            v.noteOn (note, velocity, params, glideFrom, legato, stamp);
            for (int i = 1; i < kMaxVoices; ++i) if (voices[(size_t) i].isActive()) voices[(size_t) i].noteOff();
        }
        lastNote = note;
    }

    void noteOff (int note)
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        if (sustain) { sustainedNotes.push_back (note); return; }
        releaseNote (note);
    }

    void allNotesOff()
    {
        heldNotes.clear(); sustainedNotes.clear();
        for (auto& v : voices) if (v.isActive()) v.noteOff();
    }
    void panic() { heldNotes.clear(); sustainedNotes.clear(); for (auto& v : voices) v.kill(); }

    void setSustain (bool down)
    {
        sustain = down;
        if (! down)
        {
            for (int n : sustainedNotes)
                if (std::find (heldNotes.begin(), heldNotes.end(), n) == heldNotes.end()) releaseNote (n);
            sustainedNotes.clear();
        }
    }
    void setPitchBend (float bend01) { pitchBendTarget = clampf (bend01, -1.0f, 1.0f); }
    void setModWheel (float v) { gm.modWheel = clampf (v, 0.0f, 1.0f); }
    void setAftertouch (float v) { gm.aftertouch = clampf (v, 0.0f, 1.0f); }

    // ---- Audio --------------------------------------------------------------------------
    // Renders numSamples into outL/outR (overwrites). Voices are summed and then the effects run.
    void render (float* outL, float* outR, int numSamples)
    {
        std::fill (outL, outL + numSamples, 0.0f);
        std::fill (outR, outR + numSamples, 0.0f);
        renderVoices (outL, outR, numSamples);
        FxModulation fm;
        computeFxModulation (fm);
        fx.process (outL, outR, numSamples, params, fm);
    }

    // Voices only (no effects) - used when the processor splits blocks at MIDI events
    void renderVoices (float* outL, float* outR, int numSamples)
    {
        numSamples = std::min (numSamples, maxBlock);
        pitchBendSmooth.setTarget (pitchBendTarget);
        gm.pitchBend = pitchBendSmooth.current;
        for (int i = 0; i < numSamples; ++i) pitchBendSmooth.process();
        gm.macros = params.macros;

        for (int i = 0; i < kNumLfos; ++i)
        {
            float* buf = globalLfoBuf[(size_t) i].data();
            if (params.lfo[(size_t) i].mono)
                for (int s = 0; s < numSamples; ++s) buf[s] = globalLfos[(size_t) i].process (params.lfo[(size_t) i], params.bpm, 1.0f);
            gm.globalLfo[i] = buf;
            lastGlobalLfo[(size_t) i] = buf[std::max (0, numSamples - 1)];
        }
        for (auto& v : voices) if (v.isActive()) v.render (params, gm, outL, outR, numSamples);
    }

    void renderEffects (float* outL, float* outR, int numSamples)
    {
        FxModulation fm; computeFxModulation (fm);
        fx.process (outL, outR, numSamples, params, fm);
    }

    int getActiveVoiceCount() const { int n = 0; for (const auto& v : voices) n += v.isActive() ? 1 : 0; return n; }
    int getLatency() const { return fx.getLatency (params); }
    const EffectsChain& getEffects() const { return fx; }
    float getGlobalLfoValue (int i) const { return lastGlobalLfo[(size_t) i]; }

    // Newest active voice (for UI display of envelopes / LFOs)
    const Voice* newestVoice() const
    {
        const Voice* best = nullptr;
        for (const auto& v : voices) if (v.isActive() && (best == nullptr || v.getStamp() > best->getStamp())) best = &v;
        return best;
    }

private:
    float sr = 48000.0f; int maxBlock = 512;
    SynthParams params;
    WavetableBank bank;
    std::array<Voice, kMaxVoices> voices;
    std::array<LFO, kNumLfos> globalLfos;
    std::array<std::vector<float>, kNumLfos> globalLfoBuf;
    std::array<float, kNumLfos> lastGlobalLfo { 0, 0, 0 };
    EffectsChain fx;
    GlobalModState gm;
    Smoother pitchBendSmooth; float pitchBendTarget = 0.0f;
    std::vector<int> heldNotes, sustainedNotes;
    bool sustain = false;
    int lastNote = -1;
    uint64_t stamp = 0;

    void releaseNote (int note)
    {
        if (params.voiceMode == VoiceMode::Poly)
        {
            for (auto& v : voices) if (v.isActive() && ! v.isReleasing() && v.getNote() == note) v.noteOff();
        }
        else
        {
            Voice& v = voices[0];
            if (v.getNote() != note || ! v.isActive()) return;
            if (! heldNotes.empty())
            {
                // return to the most recently held key
                const int back = heldNotes.back();
                ++stamp;
                v.noteOn (back, v.getVelocity(), params, (float) note, params.voiceMode == VoiceMode::Legato, stamp);
                lastNote = back;
            }
            else v.noteOff();
        }
    }

    Voice* allocateVoice()
    {
        const int limit = std::max (1, std::min (kMaxVoices, params.voices));
        int active = 0;
        for (auto& v : voices) active += v.isActive() ? 1 : 0;
        if (active < limit)
            for (auto& v : voices) if (! v.isActive()) return &v;

        // steal: quietest releasing voice, else the oldest
        Voice* best = nullptr; float bestLevel = 1.0e9f;
        for (auto& v : voices)
            if (v.isActive() && v.isReleasing() && v.getAmpLevel() < bestLevel) { best = &v; bestLevel = v.getAmpLevel(); }
        if (best == nullptr)
        {
            uint64_t oldest = ~0ull;
            for (auto& v : voices) if (v.isActive() && v.getStamp() < oldest) { oldest = v.getStamp(); best = &v; }
        }
        if (best == nullptr) best = &voices[0];
        return best;
    }

    void computeFxModulation (FxModulation& fm)
    {
        // Global destinations are driven by global sources plus the newest voice's per-voice sources.
        const Voice* nv = newestVoice();
        for (const auto& slot : params.mod)
        {
            if (slot.source == ModSource::None || slot.amount == 0.0f) continue;
            float v = 0.0f;
            switch (slot.source)
            {
                case ModSource::ModWheel: v = gm.modWheel; break;
                case ModSource::Aftertouch: v = gm.aftertouch; break;
                case ModSource::PitchBend: v = gm.pitchBend; break;
                case ModSource::Macro1: v = params.macros[0]; break;
                case ModSource::Macro2: v = params.macros[1]; break;
                case ModSource::Macro3: v = params.macros[2]; break;
                case ModSource::Macro4: v = params.macros[3]; break;
                case ModSource::Lfo1: case ModSource::Lfo2: case ModSource::Lfo3:
                {
                    const int i = (int) slot.source - (int) ModSource::Lfo1;
                    v = params.lfo[(size_t) i].mono ? lastGlobalLfo[(size_t) i] : (nv ? nv->getLastLfo (i) : 0.0f);
                    break;
                }
                case ModSource::Env1: case ModSource::Env2: case ModSource::Env3:
                    v = nv ? nv->getEnvLevel ((int) slot.source - (int) ModSource::Env1) : 0.0f; break;
                case ModSource::Velocity: v = nv ? nv->getVelocity() : 0.0f; break;
                case ModSource::KeyTrack: v = nv ? nv->getKeyTrack() : 0.0f; break;
                case ModSource::Random: v = nv ? nv->getRandom() : 0.0f; break;
                default: break;
            }
            v *= slot.amount;
            switch (slot.dest)
            {
                case ModDest::ChorusMix: fm.chorusMix += v; break;
                case ModDest::DelayMix: fm.delayMix += v; break;
                case ModDest::DelayFeedback: fm.delayFeedback += v; break;
                case ModDest::ReverbMix: fm.reverbMix += v; break;
                case ModDest::ReverbSize: fm.reverbSize += v; break;
                default: break;
            }
        }
    }
};

} // namespace lucid
