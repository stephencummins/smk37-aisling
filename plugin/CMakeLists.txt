# SPDX-License-Identifier: GPL-3.0-only
# AISLING as a plugin (VST3, AU, Standalone): the FM-1's engine (firmware/src/dsp/amb.c, the same C) in a DAW, to
# make a pack piece's Patch sounds by ear and save them into the pack.
#
#   cmake -S plugin -B plugin/build -DFETCHCONTENT_SOURCE_DIR_JUCE=$HOME/Projects/micgnome/plugin/build/_deps/juce-src
#   cmake --build plugin/build -j8
#
# The build copies the plugins into ~/Library/Audio/Plug-Ins (COPY_PLUGIN_AFTER_BUILD).
cmake_minimum_required(VERSION 3.22)

# Before project(): see ~/Projects/grit (a later set(... CACHE) is a no-op, and the plugin won't load under Rosetta).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

project(AislingPlugin VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# The engine: plain C, single precision, as the host tools build it (no contraction: the same numbers as the tests).
add_library(aisling_engine STATIC Source/glue.c ../firmware/src/dsp/amb.c)
target_include_directories(aisling_engine PRIVATE ../firmware/src/dsp ../firmware/src/app)
target_compile_definitions(aisling_engine PRIVATE EN_HOST _GNU_SOURCE)
target_compile_options(aisling_engine PRIVATE -O2 -ffp-contract=off)
set_target_properties(aisling_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(Aisling
    PRODUCT_NAME "AISLING"
    COMPANY_NAME "Stephen Cummins"
    BUNDLE_ID com.stephen8n.aisling
    PLUGIN_MANUFACTURER_CODE Stph
    PLUGIN_CODE Aisl
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

# The site's two faces (web/brand/fonts, as TrueType: JUCE reads no woff2), both under the Open Font License.
juce_add_binary_data(AislingAssets SOURCES
    Assets/BarlowSemiCondensed-SemiBold.ttf
    Assets/IBMPlexMono-Regular.ttf)
set_target_properties(AislingAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(Aisling PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Fm1Link.cpp)
target_compile_definitions(Aisling PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Aisling PRIVATE
    aisling_engine
    AislingAssets
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# The plugin without a host (plugin/test/host_test.cpp): cmake --build plugin/build --target AislingTest
juce_add_console_app(AislingTest PRODUCT_NAME "AislingTest")
target_sources(AislingTest PRIVATE test/host_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Fm1Link.cpp)
target_compile_definitions(AislingTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="AISLING"
    JucePlugin_Build_Standalone=1 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(AislingTest PRIVATE aisling_engine AislingAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
set_target_properties(AislingTest PROPERTIES EXCLUDE_FROM_ALL TRUE)
