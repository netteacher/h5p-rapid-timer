cmake_minimum_required(VERSION 3.22)
project(LucidSynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary for Apple Silicon + Intel")
endif()

option(LUCID_BUILD_TESTS "Build the offline DSP test executable" ON)
option(LUCID_COPY_PLUGIN "Copy the built plug-ins into the user plug-in folders" ON)

# ---------------------------------------------------------------- JUCE (fetched automatically)
include(FetchContent)
set(JUCE_VERSION 8.0.4)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_VERSION}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- Plug-in target
juce_add_plugin(LucidSynth
    PRODUCT_NAME                "LUCID"
    COMPANY_NAME                "Lucid Audio"
    BUNDLE_ID                   "com.lucidaudio.lucid"
    PLUGIN_MANUFACTURER_CODE    Lcda
    PLUGIN_CODE                 Lcd1
    FORMATS                     AU VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     ${LUCID_COPY_PLUGIN}
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    VST3_CATEGORIES             Instrument Synth
    AU_SANDBOX_SAFE             TRUE
    HARDENED_RUNTIME_ENABLED    TRUE
)

target_sources(LucidSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/Displays.cpp
    Source/ui/Panels.cpp
)

target_include_directories(LucidSynth PRIVATE Source)

target_compile_definitions(LucidSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(LucidSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------- Offline DSP tests (no JUCE)
if(LUCID_BUILD_TESTS)
    enable_testing()
    add_executable(lucid_dsp_tests tests/dsp_tests.cpp)
    target_compile_features(lucid_dsp_tests PRIVATE cxx_std_17)
    if(NOT MSVC)
        target_compile_options(lucid_dsp_tests PRIVATE -O2 -Wall -Wextra)
    endif()
    add_test(NAME lucid_dsp_tests COMMAND lucid_dsp_tests)

    # Plug-in level tests: run the real AudioProcessor headlessly (all presets, state, MIDI)
    juce_add_console_app(lucid_plugin_tests PRODUCT_NAME "lucid_plugin_tests")
    target_sources(lucid_plugin_tests PRIVATE
        tests/plugin_tests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Parameters.cpp
        Source/Presets.cpp
        Source/ui/LookAndFeel.cpp
        Source/ui/Displays.cpp
        Source/ui/Panels.cpp
    )
    target_include_directories(lucid_plugin_tests PRIVATE Source)
    target_compile_definitions(lucid_plugin_tests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(lucid_plugin_tests PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME lucid_plugin_tests COMMAND lucid_plugin_tests)
endif()
