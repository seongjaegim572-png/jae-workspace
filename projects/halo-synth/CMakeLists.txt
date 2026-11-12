cmake_minimum_required(VERSION 3.22)

project(Halo VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

set(FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FORMATS AU)
endif()

juce_add_plugin(Halo
    COMPANY_NAME "82 Jae"
    PRODUCT_NAME "Halo"
    BUNDLE_ID "com.82jae.halo"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Jae8
    PLUGIN_CODE Halo
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${FORMATS})

set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../plugin-common)
set(HALO_SOURCES
    ${COMMON_DIR}/UiKit.cpp
    ${COMMON_DIR}/Wavetables.cpp
    Source/Textures.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(Halo PRIVATE ${HALO_SOURCES})
target_include_directories(Halo PRIVATE Source ${COMMON_DIR})
target_compile_definitions(Halo PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Halo
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(BUILD_TESTS "Build the offline test app" OFF)
if(BUILD_TESTS)
    juce_add_console_app(HaloTests PRODUCT_NAME "HaloTests")
    target_sources(HaloTests PRIVATE tests/main.cpp ${HALO_SOURCES})
    target_include_directories(HaloTests PRIVATE Source ${COMMON_DIR})
    target_compile_definitions(HaloTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(HaloTests PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
endif()
