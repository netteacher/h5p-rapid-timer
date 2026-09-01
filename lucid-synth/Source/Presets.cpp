#include "Presets.h"

namespace lucid {

#define P(id, v) PresetValue { id, (float) (v) }

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", "Init", {} },

        // ------------------------------------------------------------------ PADS
        { "Glacial Pad", "Pad", {
            P("oscA_table", 1), P("oscA_unison", 6), P("oscA_detune", 0.35), P("oscA_spread", 0.9), P("oscA_level", 0.7),
            P("oscB_on", 1), P("oscB_table", 7), P("oscB_morph", 0.3), P("oscB_level", 0.35), P("oscB_unison", 3), P("oscB_detune", 0.2), P("oscB_oct", 1),
            P("f1_type", 1), P("f1_cutoff", 1800), P("f1_res", 0.12), P("f1_env", 0.3), P("f1_key", 0.3),
            P("env1_a", 0.9), P("env1_d", 1.0), P("env1_s", 0.85), P("env1_r", 2.2),
            P("env2_a", 1.5), P("env2_d", 2.0), P("env2_s", 0.5), P("env2_r", 2.0),
            P("lfo1_rate", 0.08), P("mod1_src", 4), P("mod1_dst", 4), P("mod1_amt", 0.35),
            P("lfo2_rate", 0.15), P("lfo2_shape", 6), P("mod2_src", 5), P("mod2_dst", 19), P("mod2_amt", 0.15),
            P("mod3_src", 13), P("mod3_dst", 19), P("mod3_amt", 0.6),
            P("cho_on", 1), P("cho_mix", 0.3), P("rev_on", 1), P("rev_mix", 0.4), P("rev_decay", 0.75), P("rev_size", 0.8),
            P("dly_on", 1), P("dly_mix", 0.15), P("dly_div", 6), P("dly_fb", 0.35) } },

        { "Vowel Choir", "Pad", {
            P("oscA_table", 5), P("oscA_morph", 0.4), P("oscA_unison", 5), P("oscA_detune", 0.22), P("oscA_spread", 0.8),
            P("oscB_on", 1), P("oscB_table", 13), P("oscB_oct", -1), P("oscB_level", 0.3),
            P("f1_type", 1), P("f1_cutoff", 5000), P("f1_res", 0.05),
            P("env1_a", 0.6), P("env1_s", 0.9), P("env1_r", 1.8),
            P("lfo1_rate", 0.06), P("lfo1_shape", 1), P("mod1_src", 4), P("mod1_dst", 4), P("mod1_amt", 0.5),
            P("lfo2_rate", 5.0), P("lfo2_fade", 1.2), P("mod2_src", 5), P("mod2_dst", 3), P("mod2_amt", 0.006),
            P("rev_on", 1), P("rev_mix", 0.45), P("rev_decay", 0.8), P("rev_size", 0.9), P("cho_on", 1), P("cho_mix", 0.25) } },

        { "Analog Strings", "Pad", {
            P("oscA_engine", 1), P("oscA_shape", 0), P("oscA_unison", 4), P("oscA_detune", 0.2), P("oscA_spread", 0.7), P("oscA_drift", 0.4),
            P("oscB_on", 1), P("oscB_engine", 1), P("oscB_shape", 0), P("oscB_oct", -1), P("oscB_unison", 3), P("oscB_detune", 0.25), P("oscB_level", 0.5), P("oscB_drift", 0.4),
            P("f1_type", 1), P("f1_cutoff", 3000), P("f1_key", 0.5), P("f1_env", 0.2),
            P("env1_a", 0.4), P("env1_s", 1.0), P("env1_r", 1.2), P("env2_a", 0.8), P("env2_s", 0.6),
            P("lfo1_rate", 4.5), P("lfo1_fade", 1.0), P("mod1_src", 4), P("mod1_dst", 3), P("mod1_amt", 0.008),
            P("cho_on", 1), P("cho_mix", 0.35), P("cho_rate", 0.4), P("rev_on", 1), P("rev_mix", 0.3), P("rev_decay", 0.6), P("eq_on", 1), P("eq_highGain", 2.0) } },

        { "Cinematic Swell", "Pad", {
            P("oscA_table", 10), P("oscA_morph", 0.1), P("oscA_unison", 4), P("oscA_detune", 0.3), P("oscA_spread", 1.0),
            P("oscB_on", 1), P("oscB_table", 12), P("oscB_level", 0.25), P("oscB_oct", -1),
            P("env1_a", 2.5), P("env1_s", 1.0), P("env1_r", 3.0),
            P("env3_a", 3.0), P("env3_s", 1.0), P("env3_r", 2.0), P("mod1_src", 3), P("mod1_dst", 4), P("mod1_amt", 0.8),
            P("f1_type", 1), P("f1_cutoff", 4000), P("f1_env", 0.3), P("env2_a", 2.5), P("env2_s", 1.0),
            P("noise_level", 0.08), P("noise_color", 0.3),
            P("rev_on", 1), P("rev_mix", 0.5), P("rev_decay", 0.9), P("rev_size", 1.0), P("dly_on", 1), P("dly_mix", 0.2), P("dly_div", 4), P("dly_fb", 0.5) } },

        { "Grain Drift", "Texture", {
            P("oscA_table", 12), P("oscA_unison", 3), P("oscA_detune", 0.15), P("oscA_spread", 1.0),
            P("oscB_on", 1), P("oscB_table", 9), P("oscB_morph", 0.3), P("oscB_level", 0.15), P("oscB_oct", 1),
            P("lfo1_rate", 0.03), P("lfo1_shape", 6), P("mod1_src", 4), P("mod1_dst", 4), P("mod1_amt", 0.5),
            P("f1_type", 4), P("f1_cutoff", 1200), P("f1_res", 0.35), P("lfo2_rate", 0.11), P("lfo2_shape", 1), P("mod2_src", 5), P("mod2_dst", 17), P("mod2_amt", 0.4),
            P("env1_a", 1.5), P("env1_s", 1.0), P("env1_r", 2.5),
            P("rev_on", 1), P("rev_mix", 0.6), P("rev_decay", 0.9), P("rev_size", 0.9), P("dly_on", 1), P("dly_mix", 0.3), P("dly_div", 8), P("dly_fb", 0.55) } },

        // ------------------------------------------------------------------ KEYS
        { "Lucid EP", "Keys", {
            P("oscA_table", 13), P("oscA_level", 0.8),
            P("oscB_on", 1), P("oscB_table", 13), P("oscB_oct", 2), P("oscB_level", 0.0),
            P("fm", 0.32), P("mod1_src", 3), P("mod1_dst", 12), P("mod1_amt", 0.35), P("mod2_src", 7), P("mod2_dst", 12), P("mod2_amt", 0.25),
            P("env3_a", 0.001), P("env3_d", 0.9), P("env3_s", 0.0), P("env3_r", 0.3),
            P("env1_a", 0.002), P("env1_d", 1.6), P("env1_s", 0.35), P("env1_r", 0.6), P("velAmp", 0.7),
            P("f1_on", 0), P("cho_on", 1), P("cho_mix", 0.2), P("cho_rate", 0.35), P("rev_on", 1), P("rev_mix", 0.2), P("rev_size", 0.4), P("eq_on", 1), P("eq_lowGain", 2.0) } },

        { "Glass Bells", "Keys", {
            P("oscA_table", 8), P("oscA_morph", 0.2), P("oscB_on", 1), P("oscB_table", 7), P("oscB_level", 0.3), P("oscB_oct", 1),
            P("env1_a", 0.001), P("env1_d", 2.5), P("env1_s", 0.0), P("env1_r", 1.5),
            P("env3_a", 0.001), P("env3_d", 0.6), P("env3_s", 0.0), P("mod1_src", 3), P("mod1_dst", 4), P("mod1_amt", 0.5), P("mod2_src", 3), P("mod2_dst", 7), P("mod2_amt", 0.4),
            P("f1_on", 0), P("velAmp", 0.6),
            P("rev_on", 1), P("rev_mix", 0.35), P("rev_decay", 0.7), P("dly_on", 1), P("dly_div", 6), P("dly_mix", 0.2), P("dly_fb", 0.4) } },

        { "Warm Organ", "Keys", {
            P("oscA_table", 6), P("oscA_morph", 0.7), P("oscB_on", 1), P("oscB_table", 13), P("oscB_oct", 1), P("oscB_level", 0.4),
            P("env1_a", 0.004), P("env1_s", 1.0), P("env1_r", 0.06), P("velAmp", 0.0),
            P("lfo1_rate", 6.0), P("mod1_src", 4), P("mod1_dst", 24), P("mod1_amt", 0.5), P("mod2_src", 4), P("mod2_dst", 3), P("mod2_amt", 0.004),
            P("mod3_src", 8), P("mod3_dst", 25), P("mod3_amt", 0.4),
            P("f1_type", 0), P("f1_cutoff", 6000), P("sat_on", 1), P("sat_type", 1), P("sat_drive", 0.25), P("rev_on", 1), P("rev_mix", 0.22), P("rev_size", 0.5) } },

        { "Bitcrush Toy", "Keys", {
            P("oscA_table", 9), P("oscA_morph", 0.6), P("oscB_on", 1), P("oscB_table", 0), P("oscB_morph", 0.85), P("oscB_oct", -1), P("oscB_level", 0.3),
            P("env1_a", 0.001), P("env1_d", 0.4), P("env1_s", 0.2), P("env1_r", 0.3),
            P("f1_type", 0), P("f1_cutoff", 4000), P("f1_env", 0.5), P("env2_d", 0.3), P("env2_s", 0.0),
            P("dly_on", 1), P("dly_div", 8), P("dly_mix", 0.3), P("dly_fb", 0.45), P("rev_on", 1), P("rev_mix", 0.2) } },

        // ------------------------------------------------------------------ BASS
        { "Sub Ladder Bass", "Bass", {
            P("oscA_engine", 1), P("oscA_shape", 0), P("oscB_on", 1), P("oscB_engine", 1), P("oscB_shape", 1), P("oscB_oct", -1), P("oscB_level", 0.5), P("oscB_pw", 0.4),
            P("sub_level", 0.6), P("sub_oct", 0),
            P("f1_type", 6), P("f1_cutoff", 320), P("f1_res", 0.35), P("f1_drive", 0.3), P("f1_env", 0.6), P("f1_key", 0.3),
            P("env2_a", 0.001), P("env2_d", 0.28), P("env2_s", 0.1), P("env1_a", 0.001), P("env1_d", 0.5), P("env1_s", 0.8), P("env1_r", 0.15),
            P("voiceMode", 2), P("glide", 0.05), P("sat_on", 1), P("sat_drive", 0.2), P("comp_on", 1), P("comp_thresh", -14), P("comp_ratio", 3), P("comp_makeup", 3) } },

        { "Reese", "Bass", {
            P("oscA_engine", 1), P("oscA_shape", 0), P("oscA_unison", 4), P("oscA_detune", 0.14), P("oscA_spread", 0.3), P("oscA_oct", -1),
            P("oscB_on", 1), P("oscB_engine", 1), P("oscB_shape", 0), P("oscB_oct", -1), P("oscB_fine", 12), P("oscB_level", 0.6),
            P("f1_type", 1), P("f1_cutoff", 900), P("f1_res", 0.2), P("f1_env", 0.25), P("env2_d", 0.4), P("env2_s", 0.3),
            P("env1_a", 0.005), P("env1_s", 1.0), P("env1_r", 0.2), P("voiceMode", 1),
            P("sat_on", 1), P("sat_drive", 0.4), P("eq_on", 1), P("eq_lowFreq", 80), P("eq_lowGain", 3.0), P("comp_on", 1), P("comp_thresh", -12) } },

        { "Neuro Growl", "Bass", {
            P("oscA_table", 3), P("oscA_morph", 0.3), P("oscA_oct", -1),
            P("oscB_on", 1), P("oscB_table", 2), P("oscB_morph", 0.4), P("oscB_oct", -1), P("oscB_level", 0.5),
            P("lfo1_sync", 1), P("lfo1_div", 7), P("lfo1_shape", 1), P("mod1_src", 4), P("mod1_dst", 4), P("mod1_amt", 0.45), P("mod2_src", 4), P("mod2_dst", 5), P("mod2_amt", -0.3),
            P("f1_type", 6), P("f1_cutoff", 450), P("f1_res", 0.45), P("f1_drive", 0.6), P("f1_env", 0.4),
            P("f2_on", 1), P("f2_type", 4), P("f2_cutoff", 1500), P("f2_res", 0.5), P("routing", 1), P("filterMix", 0.4),
            P("lfo2_sync", 1), P("lfo2_div", 9), P("mod3_src", 5), P("mod3_dst", 18), P("mod3_amt", 0.3),
            P("env1_a", 0.002), P("env1_s", 1.0), P("env1_r", 0.1), P("voiceMode", 1),
            P("sat_on", 1), P("sat_type", 0), P("sat_drive", 0.5), P("comp_on", 1), P("comp_thresh", -16), P("comp_ratio", 4), P("comp_makeup", 4), P("eq_on", 1), P("eq_lowGain", 2.5) } },

        { "808 Sub", "Bass", {
            P("oscA_engine", 1), P("oscA_shape", 3), P("oscB_on", 0),
            P("env3_a", 0.001), P("env3_d", 0.09), P("env3_s", 0.0), P("env3_r", 0.05), P("mod1_src", 3), P("mod1_dst", 3), P("mod1_amt", 0.6),
            P("env1_a", 0.001), P("env1_d", 1.6), P("env1_s", 0.0), P("env1_r", 0.3), P("velAmp", 0.4),
            P("f1_on", 0), P("voiceMode", 1), P("glide", 0.03),
            P("sat_on", 1), P("sat_type", 1), P("sat_drive", 0.35), P("comp_on", 1), P("comp_thresh", -10), P("comp_ratio", 2.5), P("comp_makeup", 2) } },

        // ------------------------------------------------------------------ LEADS
        { "Supersaw Lead", "Lead", {
            P("oscA_engine", 1), P("oscA_shape", 0), P("oscA_unison", 7), P("oscA_detune", 0.45), P("oscA_spread", 1.0), P("oscA_blend", 0.85),
            P("oscB_on", 1), P("oscB_engine", 1), P("oscB_shape", 0), P("oscB_oct", 1), P("oscB_level", 0.3), P("oscB_unison", 5), P("oscB_detune", 0.4),
            P("f1_type", 1), P("f1_cutoff", 6500), P("f1_res", 0.05), P("f1_env", 0.15),
            P("env1_a", 0.01), P("env1_s", 1.0), P("env1_r", 0.5),
            P("lfo1_rate", 5.5), P("lfo1_fade", 0.5), P("mod1_src", 4), P("mod1_dst", 3), P("mod1_amt", 0.01), P("mod2_src", 8), P("mod2_dst", 19), P("mod2_amt", 0.3),
            P("cho_on", 1), P("cho_mix", 0.25), P("dly_on", 1), P("dly_div", 7), P("dly_mix", 0.25), P("dly_fb", 0.35), P("rev_on", 1), P("rev_mix", 0.28), P("rev_decay", 0.6) } },

        { "Sync Screamer", "Lead", {
            P("oscA_table", 3), P("oscA_morph", 0.5),
            P("env3_a", 0.001), P("env3_d", 0.35), P("env3_s", 0.2), P("env3_r", 0.3), P("mod1_src", 3), P("mod1_dst", 4), P("mod1_amt", 0.5), P("mod2_src", 8), P("mod2_dst", 4), P("mod2_amt", 0.4),
            P("voiceMode", 2), P("glide", 0.08),
            P("f1_type", 0), P("f1_cutoff", 7000), P("f1_drive", 0.5),
            P("env1_a", 0.003), P("env1_s", 1.0), P("env1_r", 0.25),
            P("sat_on", 1), P("sat_type", 1), P("sat_drive", 0.45), P("dly_on", 1), P("dly_div", 8), P("dly_mix", 0.3), P("dly_fb", 0.4), P("rev_on", 1), P("rev_mix", 0.2) } },

        { "Fold Lead", "Lead", {
            P("oscA_table", 4), P("oscA_morph", 0.35), P("oscA_unison", 2), P("oscA_detune", 0.1),
            P("env3_a", 0.001), P("env3_d", 0.5), P("env3_s", 0.3), P("mod1_src", 3), P("mod1_dst", 4), P("mod1_amt", 0.45), P("mod2_src", 7), P("mod2_dst", 4), P("mod2_amt", 0.3),
            P("f1_type", 6), P("f1_cutoff", 2500), P("f1_res", 0.3), P("f1_env", 0.3), P("f1_key", 0.5),
            P("voiceMode", 2), P("glide", 0.06),
            P("env1_a", 0.005), P("env1_s", 0.9), P("env1_r", 0.3),
            P("sat_on", 1), P("sat_drive", 0.3), P("dly_on", 1), P("dly_div", 6), P("dly_mix", 0.25), P("rev_on", 1), P("rev_mix", 0.25) } },

        // ------------------------------------------------------------------ PLUCKS & SEQUENCES
        { "Crystal Pluck", "Pluck", {
            P("oscA_table", 6), P("oscA_morph", 0.5), P("oscB_on", 1), P("oscB_table", 11), P("oscB_morph", 0.2), P("oscB_level", 0.4),
            P("env1_a", 0.001), P("env1_d", 0.5), P("env1_s", 0.0), P("env1_r", 0.4),
            P("f1_type", 1), P("f1_cutoff", 1200), P("f1_res", 0.15), P("f1_env", 0.7), P("f1_key", 0.6), P("env2_a", 0.001), P("env2_d", 0.25), P("env2_s", 0.0),
            P("mod1_src", 7), P("mod1_dst", 17), P("mod1_amt", 0.3), P("velAmp", 0.6),
            P("dly_on", 1), P("dly_div", 7), P("dly_mix", 0.3), P("dly_fb", 0.45), P("rev_on", 1), P("rev_mix", 0.3), P("rev_decay", 0.55) } },

        { "Spectral Pluck", "Pluck", {
            P("oscA_table", 10), P("oscA_morph", 0.5), P("oscA_unison", 3), P("oscA_detune", 0.15), P("oscA_spread", 0.7),
            P("env3_a", 0.001), P("env3_d", 0.3), P("env3_s", 0.0), P("mod1_src", 3), P("mod1_dst", 4), P("mod1_amt", -0.45),
            P("env1_a", 0.001), P("env1_d", 0.7), P("env1_s", 0.0), P("env1_r", 0.5),
            P("f1_type", 0), P("f1_cutoff", 9000),
            P("dly_on", 1), P("dly_div", 8), P("dly_mix", 0.28), P("dly_fb", 0.5), P("rev_on", 1), P("rev_mix", 0.35), P("rev_decay", 0.65) } },

        { "Pigment Steps", "Sequence", {
            P("oscA_table", 0), P("oscA_morph", 0.75), P("oscB_on", 1), P("oscB_table", 2), P("oscB_oct", -1), P("oscB_level", 0.4),
            P("lfo1_sync", 1), P("lfo1_div", 9), P("lfo1_shape", 5), P("mod1_src", 4), P("mod1_dst", 4), P("mod1_amt", 0.3), P("mod2_src", 4), P("mod2_dst", 17), P("mod2_amt", 0.25),
            P("f1_type", 0), P("f1_cutoff", 1500), P("f1_res", 0.3), P("f1_env", 0.45), P("env2_d", 0.18), P("env2_s", 0.0),
            P("env1_a", 0.001), P("env1_d", 0.3), P("env1_s", 0.3), P("env1_r", 0.2),
            P("dly_on", 1), P("dly_div", 8), P("dly_mix", 0.3), P("dly_fb", 0.4), P("rev_on", 1), P("rev_mix", 0.2) } },

        { "Riser Engine", "FX", {
            P("oscA_table", 3), P("oscA_morph", 0.2), P("oscA_unison", 6), P("oscA_detune", 0.5), P("oscA_spread", 1.0),
            P("noise_level", 0.4), P("noise_color", 0.4),
            P("env3_a", 4.0), P("env3_s", 1.0), P("env3_r", 1.0), P("mod1_src", 3), P("mod1_dst", 3), P("mod1_amt", 0.5), P("mod2_src", 3), P("mod2_dst", 19), P("mod2_amt", 0.6), P("mod3_src", 3), P("mod3_dst", 4), P("mod3_amt", 0.7),
            P("f1_type", 2), P("f1_cutoff", 200), P("f1_res", 0.3),
            P("env1_a", 0.5), P("env1_s", 1.0), P("env1_r", 1.5),
            P("rev_on", 1), P("rev_mix", 0.5), P("rev_decay", 0.85), P("rev_size", 1.0), P("dly_on", 1), P("dly_mix", 0.3), P("dly_div", 7), P("dly_fb", 0.6) } },
    };
    return presets;
}
#undef P

// ------------------------------------------------------------------------------------------
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts) : state (apvts)
{
    rescanUserPresets();
}

juce::File PresetManager::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("LUCID").getChildFile ("Presets");
}

void PresetManager::rescanUserPresets()
{
    entries.clear();
    const auto& f = factoryPresets();
    for (int i = 0; i < (int) f.size(); ++i) entries.push_back ({ f[(size_t) i].name, f[(size_t) i].category, false, i, {} });
    auto folder = getUserPresetFolder();
    if (folder.isDirectory())
    {
        juce::Array<juce::File> files = folder.findChildFiles (juce::File::findFiles, false, "*.lucid");
        files.sort();
        for (auto& file : files) entries.push_back ({ file.getFileNameWithoutExtension(), "User", true, -1, file });
    }
}

juce::String PresetManager::getName (int index) const { return juce::isPositiveAndBelow (index, (int) entries.size()) ? entries[(size_t) index].name : juce::String(); }
juce::String PresetManager::getCategory (int index) const { return juce::isPositiveAndBelow (index, (int) entries.size()) ? entries[(size_t) index].category : juce::String(); }
bool PresetManager::isUserPreset (int index) const { return juce::isPositiveAndBelow (index, (int) entries.size()) && entries[(size_t) index].user; }

void PresetManager::resetAllToDefault()
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void PresetManager::applyValues (const std::vector<PresetValue>& values)
{
    for (const auto& v : values)
    {
        if (auto* p = state.getParameter (v.id))
            p->setValueNotifyingHost (p->convertTo0to1 (v.value));
        else
            jassertfalse; // unknown parameter id in a factory preset
    }
}

void PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) entries.size())) return;
    const auto& e = entries[(size_t) index];
    if (e.user)
    {
        if (auto xml = juce::XmlDocument::parse (e.file))
        {
            if (xml->hasTagName (state.state.getType()))
            {
                auto tree = juce::ValueTree::fromXml (*xml);
                // apply through the parameters so the host sees the change
                resetAllToDefault();
                for (int i = 0; i < tree.getNumChildren(); ++i)
                {
                    auto child = tree.getChild (i);
                    if (child.hasType ("PARAM"))
                        if (auto* p = state.getParameter (child.getProperty ("id").toString()))
                            p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
                }
            }
        }
    }
    else
    {
        resetAllToDefault();
        applyValues (factoryPresets()[(size_t) e.factoryIndex].values);
    }
    currentIndex = index;
    currentName = e.name;
    notify();
}

void PresetManager::loadNext() { loadPreset ((currentIndex + 1) % std::max (1, getNumPresets())); }
void PresetManager::loadPrevious() { loadPreset ((currentIndex - 1 + getNumPresets()) % std::max (1, getNumPresets())); }
void PresetManager::loadInit() { loadPreset (0); }

bool PresetManager::saveUserPreset (const juce::String& nameIn)
{
    auto name = juce::File::createLegalFileName (nameIn.trim());
    if (name.isEmpty()) return false;
    auto folder = getUserPresetFolder();
    folder.createDirectory();
    auto file = folder.getChildFile (name + ".lucid");
    auto tree = state.copyState();
    tree.setProperty ("presetName", name, nullptr);
    if (auto xml = tree.createXml())
    {
        if (! xml->writeTo (file)) return false;
    }
    rescanUserPresets();
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].user && entries[(size_t) i].name == name) currentIndex = i;
    currentName = name;
    notify();
    return true;
}

void PresetManager::stateRestored (const juce::String& name)
{
    currentName = name.isNotEmpty() ? name : "Init";
    for (int i = 0; i < (int) entries.size(); ++i) if (entries[(size_t) i].name == currentName) { currentIndex = i; break; }
    notify();
}

void PresetManager::notify() { if (onPresetChanged) onPresetChanged(); }

} // namespace lucid
