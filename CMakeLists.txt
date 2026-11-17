# FxSound for Linux - GUI application.
# The DSP + PipeWire engine are built by the Makefile (build/libfxdsp.a, build/libfxengine.a);
# tools/build.sh runs both. Licensed AGPL-3.0-or-later like upstream FxSound.
cmake_minimum_required(VERSION 3.22)

# Linux release version: upstream 1.2.15 + Linux port revision (GitHub release tags: v1.2.15.1, ...)
set(FX_VERSION "1.2.15.1")
project(FxSoundLinux VERSION ${FX_VERSION} LANGUAGES C CXX)

set(JUCE_DIR "$ENV{HOME}/third_party/JUCE" CACHE PATH "JUCE 7 source tree")
set(FX_MAKE_BUILD "${CMAKE_SOURCE_DIR}/build" CACHE PATH "Makefile build dir with libfxdsp.a / libfxengine.a")
add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
find_package(X11 REQUIRED)

set(UP ${CMAKE_SOURCE_DIR}/upstream/fxsound)

juce_add_gui_app(FxSound
    PRODUCT_NAME "FxSound"
    COMPANY_NAME "FxSound LLC"
    VERSION ${FX_VERSION}
    NEEDS_CURL TRUE
    NEEDS_WEB_BROWSER FALSE)

file(GLOB FX_GUI_SOURCES ${UP}/Source/GUI/*.cpp)
target_sources(FxSound PRIVATE
    ${FX_GUI_SOURCES}
    ${UP}/Source/Main.cpp
    ${UP}/Source/Utils/Settings/DeviceConfig.cpp
    ${UP}/Source/Utils/Settings/Settings.cpp
    ${UP}/JuceLibraryCode/BinaryData.cpp
    linux/AudioPassthruLinux.cpp
    linux/FxLinuxLog.cpp
    linux/FxLinuxFeatures.cpp
    linux/FxLinuxTray.cpp
    linux/FxX11Hotkeys.cpp
    linux/FxSystemTrayViewLinux.cpp
    linux/SysInfoLinux.cpp)

# Tray/window icons (converted from the Windows .ico files by tools/make_icons.sh)
juce_add_binary_data(FxLinuxIcons
    HEADER_NAME FxLinuxIconData.h
    NAMESPACE FxLinuxIconData
    SOURCES linux/icons/red_logo.png linux/icons/blue_logo.png linux/icons/white_logo.png linux/icons/gray_logo.png)
set_target_properties(FxLinuxIcons PROPERTIES POSITION_INDEPENDENT_CODE ON)

string(REPLACE "." ";" _v ${FX_VERSION})
list(GET _v 0 _v0)
list(GET _v 1 _v1)
list(GET _v 2 _v2)
math(EXPR FX_VERSION_NUMBER "(${_v0} << 16) | (${_v1} << 8) | ${_v2}")

target_include_directories(FxSound PRIVATE
    linux                         # Linux AudioPassthru.h / JuceHeader.h shadow the Windows ones
    ${UP}/JuceLibraryCode         # BinaryData.h (upstream-generated: fonts, images, translations)
    ${UP}/Source
    ${UP}/Source/GUI
    ${UP}/Source/Utils/Settings
    ${UP}/Source/Utils/SysInfo
    ${CMAKE_SOURCE_DIR}/upstream/dsp/include
    ${CMAKE_SOURCE_DIR}/engine)

target_compile_definitions(FxSound PRIVATE
    UNICODE=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_DARK_SPLASH_SCREEN=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_CURL=1
    JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
    JUCE_WEB_BROWSER=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FxSound,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:FxSound,JUCE_VERSION>"
    FX_VERSION_STRING="${FX_VERSION}"
    FX_VERSION_NUMBER=${FX_VERSION_NUMBER})

target_compile_options(FxSound PRIVATE "SHELL:-include ${CMAKE_SOURCE_DIR}/linux/FxGuiPrelude.h" -fpermissive -Wno-deprecated-declarations -Wno-multichar)

target_link_libraries(FxSound PRIVATE
    FxLinuxIcons
    ${FX_MAKE_BUILD}/libfxengine.a
    ${FX_MAKE_BUILD}/libfxdsp.a
    PkgConfig::PIPEWIRE
    ${X11_LIBRARIES}
    juce::juce_core juce::juce_cryptography juce::juce_data_structures juce::juce_events
    juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    dl pthread)
