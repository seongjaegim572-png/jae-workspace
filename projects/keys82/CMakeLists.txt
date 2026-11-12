cmake_minimum_required(VERSION 3.22)

project(Keys82 VERSION 0.1.0)

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

juce_add_plugin(Keys82
    COMPANY_NAME "82 Jae"
    PRODUCT_NAME "82 Keys"
    BUNDLE_ID "com.82jae.keys82"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Jae8
    PLUGIN_CODE Ky82
    VST3_CATEGORIES Instrument Piano
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${FORMATS})

set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../plugin-common)
set(KEYS_SOURCES
    ${COMMON_DIR}/UiKit.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(Keys82 PRIVATE ${KEYS_SOURCES})
target_include_directories(Keys82 PRIVATE Source ${COMMON_DIR})
target_compile_definitions(Keys82 PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Keys82
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(BUILD_TESTS "Build the offline test app" OFF)
if(BUILD_TESTS)
    juce_add_console_app(Keys82Tests PRODUCT_NAME "Keys82Tests")
    target_sources(Keys82Tests PRIVATE tests/main.cpp ${KEYS_SOURCES})
    target_include_directories(Keys82Tests PRIVATE Source ${COMMON_DIR})
    target_compile_definitions(Keys82Tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(Keys82Tests PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
endif()
