// LUCID Synth - factory presets and user preset management
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace lucid {

struct PresetValue { const char* id; float value; };
struct FactoryPreset { const char* name; const char* category; std::vector<PresetValue> values; };

const std::vector<FactoryPreset>& factoryPresets();

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);

    int getNumPresets() const { return (int) entries.size(); }
    juce::String getName (int index) const;
    juce::String getCategory (int index) const;
    bool isUserPreset (int index) const;
    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; }

    void loadPreset (int index);
    void loadNext();
    void loadPrevious();
    void loadInit();
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    static juce::File getUserPresetFolder();

    // Called by the processor when host state is restored
    void stateRestored (const juce::String& name);

    std::function<void()> onPresetChanged;

private:
    struct Entry { juce::String name, category; bool user = false; int factoryIndex = -1; juce::File file; };
    juce::AudioProcessorValueTreeState& state;
    std::vector<Entry> entries;
    int currentIndex = 0;
    juce::String currentName { "Init" };

    void resetAllToDefault();
    void applyValues (const std::vector<PresetValue>& values);
    void notify();
};

} // namespace lucid
