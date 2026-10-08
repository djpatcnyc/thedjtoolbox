cmake_minimum_required(VERSION 3.22)

project(ShiftFxPro VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is expected at <project>/JUCE (cloned at the 8.0.7 tag locally,
# and cloned by CI in the GitHub workflow). It is intentionally NOT
# committed to the repo.
add_subdirectory(JUCE)

# Audio Units only exist on macOS; guard so Linux/Windows configure cleanly.
if(APPLE)
    set(TFX_FORMATS VST3 AU)
else()
    set(TFX_FORMATS VST3)
endif()

juce_add_plugin(ShiftFxPro
    COMPANY_NAME "DJ Toolbox"
    BUNDLE_ID com.djtoolbox.shiftfxpro
    PLUGIN_MANUFACTURER_CODE Djtb      # unique 4-char manufacturer code
    PLUGIN_CODE ShFx                   # unique 4-char plugin code
    FORMATS ${TFX_FORMATS}
    PRODUCT_NAME "SHIFT FX PRO"        # UI renders it as "SHIFT / FX PRO"
    VST3_CATEGORIES "Fx"
    AU_MAIN_TYPE "kAudioUnitType_Effect"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE              # MIDI learn needs MIDI input
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(ShiftFxPro PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(ShiftFxPro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ShiftFxPro PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

#---- Offline QA harness (headless console app, reuses the exact DSP code) ----
add_executable(ShiftFxQA
    qa/qa_main.cpp
    Source/PluginProcessor.cpp
)

target_compile_definitions(ShiftFxQA PRIVATE
    SHIFTFX_HEADLESS=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(ShiftFxQA PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

#---- Offscreen UI render (dev tool: real editor -> PNG, no display needed) ----
add_executable(ShiftFxShot
    qa/shot_main.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(ShiftFxShot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(ShiftFxShot PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
