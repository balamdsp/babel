cmake_minimum_required(VERSION 3.22)
project(Babel VERSION 0.1.1)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
)
FetchContent_MakeAvailable(JUCE)

add_subdirectory(libs/clap-juce-extensions EXCLUDE_FROM_ALL)

option(BABEL_COPY_AFTER_BUILD "Copy built plugins into the system plugin folders" ON)

if (BABEL_COPY_AFTER_BUILD)
    set_property(GLOBAL PROPERTY JUCE_COPY_PLUGIN_AFTER_BUILD TRUE)
endif()

juce_add_plugin(Babel
    COMPANY_NAME "BalamDSP"
    PLUGIN_MANUFACTURER_CODE "Bala"
    PLUGIN_CODE "Babe"
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "Babel"
    VERSION ${PROJECT_VERSION}
    PLUGIN_DESCRIPTION "MIDI grid controller"
    VST3_CATEGORIES "Instrument" "Tools"
    LV2URI "https://balamdsp.com/plugins/Babel"
    AAX_CATEGORY "AAX_ePlugInCategory_SWGenerators"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
)

clap_juce_extensions_plugin(TARGET Babel
    CLAP_ID "com.balamdsp.Babel"
    CLAP_FEATURES "midi-effect" "note-effect" "utility")

juce_generate_juce_header(Babel)

target_compile_definitions(Babel
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WIN_PER_MONITOR_DPI_AWARE=0
)

target_sources(Babel
    PRIVATE
        source/Components/CustomLookAndFeel.h
        source/Components/PadGrid.h
        source/Components/PadGrid.cpp
        source/Core/ScaleQuantizer.h
        source/Core/MidiRecorder.h
        source/Helpers/BabelDefines.h
        source/Helpers/PresetManager.h
        source/Helpers/PresetManager.cpp
        source/Helpers/BabelFrameExtents.cpp
        source/PluginProcessor.h
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/PluginEditor.h
)

# BabelFrameExtents.cpp talks Xlib directly (Linux standalone frame).
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(Babel PRIVATE X11)
endif()

juce_add_binary_data(Babel_BinaryData
    HEADER_NAME "BinaryData.h"
    NAMESPACE "BinaryData"
    SOURCES
        source/Fonts/VT323-Regular.ttf
)

target_link_libraries(Babel PRIVATE Babel_BinaryData)

# Custom standalone entry (native OS title bar).
target_compile_definitions(Babel_Standalone PRIVATE
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
)
target_sources(Babel_Standalone PRIVATE
    source/Standalone/CustomStandalone.cpp
)

target_link_libraries(Babel
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_processors_headless
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
