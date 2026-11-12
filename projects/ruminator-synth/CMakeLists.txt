cmake_minimum_required(VERSION 3.22)

project(Ruminator VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local copy if JUCE_DIR is given, otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(RUMINATOR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RUMINATOR_FORMATS AU)
endif()

juce_add_plugin(Ruminator
    COMPANY_NAME "82 Jae"
    PRODUCT_NAME "Ruminator"
    BUNDLE_ID "com.82jae.ruminator"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Jae8
    PLUGIN_CODE Rumn
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${RUMINATOR_FORMATS})

set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../plugin-common)

set(RUMINATOR_SOURCES
    Source/Params.cpp
    ${COMMON_DIR}/Wavetables.cpp
    ${COMMON_DIR}/UiKit.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(Ruminator PRIVATE ${RUMINATOR_SOURCES})
target_include_directories(Ruminator PRIVATE ${COMMON_DIR})

target_compile_definitions(Ruminator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Ruminator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline test: renders every preset to a WAV and takes a GUI screenshot.
option(RUMINATOR_BUILD_TESTS "Build the offline test app" OFF)
if(RUMINATOR_BUILD_TESTS)
    juce_add_console_app(RuminatorTests PRODUCT_NAME "RuminatorTests")
    target_sources(RuminatorTests PRIVATE tests/main.cpp ${RUMINATOR_SOURCES})
    target_include_directories(RuminatorTests PRIVATE Source ${COMMON_DIR})
    target_compile_definitions(RuminatorTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(RuminatorTests
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
