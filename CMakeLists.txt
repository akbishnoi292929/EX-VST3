cmake_minimum_required(VERSION 3.22)
project(EX VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET juce::juce_audio_utils)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 7.0.12
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

juce_add_plugin(EX
    COMPANY_NAME "HeartlessAudio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Htla
    PLUGIN_CODE Ex01
    FORMATS VST3
    PRODUCT_NAME "EX"
)

# Disable manifest generation safely in JUCE 7
set_target_properties(EX PROPERTIES JUCE_VST3_CAN_REPLACE_VST2 OFF)

# Link ONNX Runtime if present
if(DEFINED ONNXRUNTIME_ROOT_DIR AND EXISTS "${ONNXRUNTIME_ROOT_DIR}")
    include_directories("${ONNXRUNTIME_ROOT_DIR}/include")
    link_directories("${ONNXRUNTIME_ROOT_DIR}/lib")
    target_link_libraries(EX PRIVATE "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.lib")
    target_compile_definitions(EX PRIVATE HAS_ONNXRUNTIME=1)
endif()

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

target_sources(EX PRIVATE ${SourceFiles})

target_compile_definitions(EX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(EX PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
)

juce_generate_juce_header(EX)
