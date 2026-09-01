// LUCID Synth - plain parameter snapshot passed from the plugin to the engine each block.
// No JUCE dependency: the engine and tests only see this struct.
#pragma once
#include <array>

namespace lucid {

enum class OscEngine : int { Wavetable = 0, Analog = 1 };
enum class AnalogShape : int { Saw = 0, Square = 1, Triangle = 2, Sine = 3 };
enum class SubShape : int { Sine = 0, Triangle = 1, Square = 2 };
enum class FilterType : int { LP12 = 0, LP24, HP12, HP24, BP12, Notch, Ladder, NumTypes };
enum class FilterRouting : int { Serial = 0, Parallel = 1, Split = 2 };
enum class VoiceMode : int { Poly = 0, Mono = 1, Legato = 2 };
enum class LfoShape : int { Sine = 0, Triangle, SawUp, SawDown, Square, SampleHold, SmoothRandom, NumShapes };

enum class ModSource : int
{
    None = 0, Env1, Env2, Env3, Lfo1, Lfo2, Lfo3, Velocity, ModWheel, Aftertouch,
    KeyTrack, PitchBend, Random, Macro1, Macro2, Macro3, Macro4, NumSources
};

enum class ModDest : int
{
    None = 0,
    OscAPitch, OscBPitch, PitchAll, OscAMorph, OscBMorph, OscALevel, OscBLevel,
    OscAPW, OscBPW, OscAPan, OscBPan, FmAmount, OscADetune, OscBDetune,
    SubLevel, NoiseLevel,
    Filter1Cutoff, Filter2Cutoff, CutoffAll, Filter1Res, Filter2Res, FilterDrive,
    AmpLevel, AmpPan,
    Lfo1Rate, Lfo2Rate, Lfo3Rate, Env2Amount,
    ChorusMix, DelayMix, DelayFeedback, ReverbMix, ReverbSize,
    NumDests
};

constexpr int kNumOscs      = 2;
constexpr int kNumFilters   = 2;
constexpr int kNumEnvs      = 3;
constexpr int kNumLfos      = 3;
constexpr int kNumModSlots  = 12;
constexpr int kNumMacros    = 4;
constexpr int kMaxUnison    = 8;
constexpr int kMaxVoices    = 32;
constexpr int kNumSyncDivs  = 13;

// Tempo sync divisions (in beats): 8 bars .. 1/32 triplet
inline float syncDivisionBeats (int idx)
{
    static const float divs[kNumSyncDivs] = { 32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.75f, 0.5f, 0.3333333f, 0.25f, 0.1666667f, 0.125f, 0.0833333f };
    return divs[idx < 0 ? 0 : (idx >= kNumSyncDivs ? kNumSyncDivs - 1 : idx)];
}

struct OscParams
{
    bool  enabled = true;
    OscEngine engine = OscEngine::Wavetable;
    int   table = 0;
    float morph = 0.0f;          // 0..1
    AnalogShape shape = AnalogShape::Saw;
    float pulseWidth = 0.5f;     // 0.05..0.95
    int   octave = 0;            // -3..3
    int   semi = 0;              // -12..12
    float fine = 0.0f;           // cents -100..100
    float level = 0.8f;          // 0..1
    float pan = 0.0f;            // -1..1
    int   unison = 1;            // 1..8
    float detune = 0.2f;         // 0..1  (scaled to cents internally)
    float spread = 0.5f;         // 0..1 stereo spread
    float blend = 0.7f;          // 0..1 side voice level
    float phase = 0.0f;          // 0..1 start phase
    bool  retrigger = false;     // reset phase on note on
    float drift = 0.0f;          // 0..1 analog drift amount
};

struct FilterParams
{
    bool  enabled = true;
    FilterType type = FilterType::LP24;
    float cutoff = 8000.0f;      // Hz 20..20000
    float resonance = 0.1f;      // 0..1
    float drive = 0.0f;          // 0..1
    float keyTrack = 0.0f;       // 0..1
    float envAmount = 0.0f;      // -1..1 (Env2), in octaves * 5
};

struct EnvParams
{
    float attack = 0.005f;       // seconds
    float decay = 0.2f;
    float sustain = 0.8f;        // 0..1
    float release = 0.3f;
    float curve = 0.5f;          // 0 = linear .. 1 = exponential (analog)
    float velocity = 0.5f;       // 0..1 velocity sensitivity
};

struct LfoParams
{
    LfoShape shape = LfoShape::Sine;
    float rateHz = 2.0f;         // 0.01..50
    bool  sync = false;
    int   syncDiv = 7;           // index into syncDivisionBeats
    float phase = 0.0f;          // 0..1
    float fadeIn = 0.0f;         // seconds
    bool  mono = false;          // true = global, false = per voice (retrigger)
    float smooth = 0.0f;         // 0..1 output smoothing
};

struct ModSlot
{
    ModSource source = ModSource::None;
    ModDest dest = ModDest::None;
    float amount = 0.0f;         // -1..1
};

struct FxParams
{
    // Saturator
    bool  satOn = false; int satType = 0; float satDrive = 0.3f; float satMix = 1.0f;
    // EQ (low shelf, peak, high shelf)
    bool  eqOn = false;
    float eqLowFreq = 120.0f, eqLowGain = 0.0f;
    float eqMidFreq = 1200.0f, eqMidGain = 0.0f, eqMidQ = 1.0f;
    float eqHighFreq = 8000.0f, eqHighGain = 0.0f;
    // Compressor
    bool  compOn = false; float compThreshold = -18.0f; float compRatio = 3.0f;
    float compAttack = 0.01f; float compRelease = 0.15f; float compMakeup = 0.0f; float compMix = 1.0f;
    // Chorus
    bool  chorusOn = false; float chorusRate = 0.6f; float chorusDepth = 0.5f; float chorusMix = 0.35f; float chorusWidth = 1.0f;
    // Delay
    bool  delayOn = false; bool delaySync = true; int delayDiv = 7; float delayTimeMs = 375.0f;
    float delayFeedback = 0.4f; float delayMix = 0.25f; bool delayPingPong = true;
    float delayLowCut = 150.0f; float delayHighCut = 6000.0f;
    // Reverb
    bool  reverbOn = false; float reverbSize = 0.6f; float reverbDecay = 0.5f; float reverbDamping = 0.5f;
    float reverbPreDelay = 20.0f; float reverbMix = 0.25f; float reverbWidth = 1.0f;
    // Limiter
    bool  limiterOn = true; float limiterCeiling = -0.3f;
};

struct SynthParams
{
    float masterGain = 0.0f;                 // dB
    int   voices = 16;                        // max polyphony
    VoiceMode voiceMode = VoiceMode::Poly;
    float glideTime = 0.0f;                  // seconds
    float pitchBendRange = 2.0f;             // semitones
    std::array<float, kNumMacros> macros { 0.0f, 0.0f, 0.0f, 0.0f };

    std::array<OscParams, kNumOscs> osc;
    float fmAmount = 0.0f;                    // B -> A phase modulation 0..1
    float ringMod = 0.0f;                     // A*B blend 0..1
    float subLevel = 0.0f; int subOctave = 1; SubShape subShape = SubShape::Sine;
    float noiseLevel = 0.0f; float noiseColor = 0.0f; // -1 dark .. 1 bright

    std::array<FilterParams, kNumFilters> filter;
    FilterRouting routing = FilterRouting::Serial;
    float filterMix = 0.5f;                   // parallel mix F1<->F2

    std::array<EnvParams, kNumEnvs> env;
    std::array<LfoParams, kNumLfos> lfo;
    std::array<ModSlot, kNumModSlots> mod;

    float ampLevel = 1.0f;                    // 0..1 (voice amp)
    float ampPan = 0.0f;
    float ampVelocity = 0.5f;                 // velocity -> amp
    FxParams fx;

    double bpm = 120.0;
};

} // namespace lucid
