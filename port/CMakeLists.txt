# Native (clang, 64-bit) build of the GTA2 decompilation for macOS and iOS.
#   cmake -S port -B build-mac -G Ninja && cmake --build build-mac
cmake_minimum_required(VERSION 3.20)
project(gta2_native CXX C OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(GTA2_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(PORT_IOS ON)
endif()

if(PORT_IOS)
    # iOS: SDL3 built from source as a static library
    set(GTA2_SDL_SOURCE ${GTA2_ROOT_DIR}/../SDL CACHE PATH "SDL3 source tree")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${GTA2_SDL_SOURCE} sdl EXCLUDE_FROM_ALL)
else()
    find_package(SDL3 REQUIRED CONFIG)
endif()

# Game sources: everything the original links, minus Windows-only tooling and the ImGui overlay
file(GLOB GAME_SOURCES ${GTA2_ROOT_DIR}/Source/*.cpp)
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/(ExePatcher|HookLoader|cl_proxy|WinMainProxy|ImGuiDebug)\\.cpp$")

set(PLATFORM_SOURCES
    platform/main_sdl.cpp
    platform/win32_shim.cpp
    platform/paths.cpp
    platform/wstr.cpp
    platform/ptr32.cpp
    platform/video_sdl.cpp
    platform/input_sdl.cpp
    platform/touch_sdl.cpp
    platform/direct_control.cpp
    platform/stubs.cpp
    platform/net_dplay.cpp
    platform/net_lobby.cpp
    platform/audio_sdl.cpp
    platform/bink_port.cpp
    platform/avf_video.mm
    platform/dx_guids.cpp
    platform/imgui_stub.cpp
)

# Flags shared by game and platform code
set(PORT_FLAGS
    -fshort-wchar          # UTF-16 GXT text kept as wchar_t
    -fms-extensions        # anonymous structs, __int64, ...
    -fno-strict-aliasing   # decompiled code type-puns freely
    -fwrapv                # MSVC-like signed overflow
    -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/port_prelude.h
)

if(PORT_IOS)
    set(IOS_RESOURCES ios/LaunchScreen.storyboard ios/Assets.xcassets)
    add_executable(gta2 MACOSX_BUNDLE ${GAME_SOURCES} ${PLATFORM_SOURCES} ${IOS_RESOURCES})
    set_source_files_properties(${IOS_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set(PORT_TEAM_ID "5QWR5WX9N7" CACHE STRING "Apple development team (yours: -DPORT_TEAM_ID=...)")
    set(PORT_BUNDLE_ID "com.erkut.gta2port" CACHE STRING "App bundle identifier")
    set_target_properties(gta2 PROPERTIES
        OUTPUT_NAME GTA2
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PORT_BUNDLE_ID}"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${PORT_TEAM_ID}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
        XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL[variant=Release] "2"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
else()
    add_executable(gta2 ${GAME_SOURCES} ${PLATFORM_SOURCES})
endif()
# Touch button icons are rendered from SF Symbols on the build Mac (the symbol images may
# not be redistributed, so the generated header is not part of the repository)
set(PORT_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
if(${CMAKE_CURRENT_SOURCE_DIR}/tools/icons.swift IS_NEWER_THAN ${PORT_GEN_DIR}/touch_icons.h)
    file(MAKE_DIRECTORY ${PORT_GEN_DIR})
    execute_process(
        COMMAND xcrun --sdk macosx swift ${CMAKE_CURRENT_SOURCE_DIR}/tools/icons.swift ${PORT_GEN_DIR}/touch_icons.h
        RESULT_VARIABLE ICONS_RESULT)
    if(NOT ICONS_RESULT EQUAL 0)
        message(FATAL_ERROR "Generating touch icons (tools/icons.swift) failed")
    endif()
endif()
target_include_directories(gta2 PRIVATE
    ${PORT_GEN_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${GTA2_ROOT_DIR}/Source
    ${GTA2_ROOT_DIR})
target_compile_definitions(gta2 PRIVATE GTA2_PORT=1 _CRT_SECURE_NO_WARNINGS)
target_compile_options(gta2 PRIVATE ${PORT_FLAGS})
set_source_files_properties(${GAME_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wno-everything")
set_source_files_properties(${PLATFORM_SOURCES} PROPERTIES COMPILE_DEFINITIONS PORT_PLATFORM_CODE=1)
# Objective-C++ glue: no Win32 prelude, normal wchar_t, ARC
set_source_files_properties(platform/avf_video.mm PROPERTIES
    COMPILE_DEFINITIONS "PORT_PLATFORM_CODE=1;PORT_NO_PRELUDE=1"
    COMPILE_OPTIONS "-fno-short-wchar;-fobjc-arc")
target_link_libraries(gta2 PRIVATE "-framework AVFoundation" "-framework CoreMedia" "-framework CoreVideo" "-framework Foundation")
if(PORT_IOS)
    target_link_libraries(gta2 PRIVATE SDL3::SDL3-static)
else()
    target_link_libraries(gta2 PRIVATE SDL3::SDL3)
endif()

option(PORT_ASAN "Build with AddressSanitizer (finds 64-bit layout bugs)" OFF)
if(PORT_ASAN)
    target_compile_options(gta2 PRIVATE -fsanitize=address -fsanitize-recover=address -fno-omit-frame-pointer)
    target_link_options(gta2 PRIVATE -fsanitize=address)
endif()
