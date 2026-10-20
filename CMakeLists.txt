cmake_minimum_required(VERSION 3.22)

# Binaires universels Mac (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(MixPilot VERSION 1.0.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# JUCE est téléchargé automatiquement au premier build
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

# Formats : AU uniquement sur Mac. Sous Windows, la copie automatique dans
# "C:/Program Files/Common Files/VST3" demande les droits administrateur : on la désactive.
if(APPLE)
    set(MIXPILOT_FORMATS VST3 AU)
    set(MIXPILOT_COPY TRUE)
elseif(UNIX)
    set(MIXPILOT_FORMATS VST3 LV2)
    set(MIXPILOT_COPY FALSE)
else()
    set(MIXPILOT_FORMATS VST3)
    set(MIXPILOT_COPY FALSE)
endif()

set(MIXPILOT_SHARED_SOURCES
    Source/Shared/SharedBus.cpp
)

function(mixpilot_configure target)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    if(MSVC)
        # les sources sont en UTF-8 (textes français accentués)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)
endfunction()

# ---------------------------------------------------------------------------
#  MixPilot Master  (à mettre sur la piste Master)
# ---------------------------------------------------------------------------
juce_add_plugin(MixPilotMaster
    COMPANY_NAME                "Rems"
    BUNDLE_ID                   com.rems.mixpilot.master
    PLUGIN_MANUFACTURER_CODE    Rems
    PLUGIN_CODE                 Mpms
    LV2URI                      "https://remsplugins.github.io/mixpilot/master"
    FORMATS                     ${MIXPILOT_FORMATS}
    PRODUCT_NAME                "MixPilot Master"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${MIXPILOT_COPY}
    VST3_CATEGORIES             Fx Mastering
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(MixPilotMaster PRIVATE
    Source/Master/MasterEntry.cpp
    Source/Master/MasterProcessor.cpp
    Source/Master/MasterEditor.cpp
    ${MIXPILOT_SHARED_SOURCES})
mixpilot_configure(MixPilotMaster)

# ---------------------------------------------------------------------------
#  MixPilot Track  (à mettre sur chaque piste)
# ---------------------------------------------------------------------------
juce_add_plugin(MixPilotTrack
    COMPANY_NAME                "Rems"
    BUNDLE_ID                   com.rems.mixpilot.track
    PLUGIN_MANUFACTURER_CODE    Rems
    PLUGIN_CODE                 Mptr
    LV2URI                      "https://remsplugins.github.io/mixpilot/track"
    FORMATS                     ${MIXPILOT_FORMATS}
    PRODUCT_NAME                "MixPilot Track"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${MIXPILOT_COPY}
    VST3_CATEGORIES             Fx Dynamics EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect)

target_sources(MixPilotTrack PRIVATE
    Source/Track/TrackEntry.cpp
    Source/Track/TrackProcessor.cpp
    Source/Track/TrackEditor.cpp
    ${MIXPILOT_SHARED_SOURCES})
mixpilot_configure(MixPilotTrack)
