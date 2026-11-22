cmake_minimum_required(VERSION 3.22)

# Solo afecta a macOS: universal (Apple Silicon + Intel), macOS 11 o superior
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(MonoCheck VERSION 1.8.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

# Formatos del plugin: VST3 en todos los sistemas; AU solo en macOS.
# La app independiente solo se compila en macOS.
if(APPLE)
    option(COPY_AFTER_BUILD "Copiar el plugin a la carpeta de plugins" ON)
    option(BUILD_APP "Compilar tambien la app independiente" ON)
    set(PLUGIN_FORMATS VST3 AU)
else()
    option(COPY_AFTER_BUILD "Copiar el plugin a la carpeta de plugins" OFF)
    option(BUILD_APP "Compilar tambien la app independiente" OFF)
    set(PLUGIN_FORMATS VST3)
endif()

# --------------------------------------------------------------------------
# Plugin (VST3 + AU)
# --------------------------------------------------------------------------
juce_add_plugin(MonoCheck
    COMPANY_NAME "MiEstudio"
    PLUGIN_MANUFACTURER_CODE Mest
    PLUGIN_CODE Mchk
    FORMATS ${PLUGIN_FORMATS}
    VST3_CATEGORIES Fx Analyzer
    PRODUCT_NAME "MonoCheck"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD ${COPY_AFTER_BUILD})

target_sources(MonoCheck PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MonoCheckView.cpp)

target_compile_definitions(MonoCheck PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MonoCheck PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_basics
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# --------------------------------------------------------------------------
# App independiente para Mac (sin DAW)
# --------------------------------------------------------------------------
if(BUILD_APP)
    juce_add_gui_app(MonoCheckApp
        PRODUCT_NAME "MonoCheck"
        COMPANY_NAME "TxuSo"
        BUNDLE_ID com.txuso.monocheck
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "MonoCheck necesita acceder a la entrada de audio para detectar si la señal es mono o estéreo.")

    target_sources(MonoCheckApp PRIVATE
        Source/AppMain.cpp
        Source/MonoCheckView.cpp)

    target_compile_definitions(MonoCheckApp PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MonoCheckApp,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MonoCheckApp,JUCE_VERSION>")

    target_link_libraries(MonoCheckApp
        PRIVATE
            juce::juce_audio_utils
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
