# Untrunc - Android library build (NDK toolchain)
#
# Produces libuntrunc.so wrapping the core repair engine (untrunc_api).
# FFmpeg is consumed as prebuilt shared libs from a local Android build
# (e.g. /d/android/workspaces/ffmpeg/output/android/arm64-v8a).
#
# Usage (standalone check):
#   cmake -B build/android-arm64 -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
#     -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 \
#     -DANDROID_NDK=D:/android/sdk/ndk/27.0.12077973 \
#     -DUNTRUNC_FFMPEG_DIR=D:/android/workspaces/ffmpeg/output/android/arm64-v8a
#   cmake --build build/android-arm64
#
# Or via gradle externalNativeBuild (see android-lib/ module).

cmake_minimum_required(VERSION 3.22)
project(untrunc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# FFmpeg prebuilt location (headers + .so), overridable via -D or gradle
if(NOT UNTRUNC_FFMPEG_DIR)
    set(UNTRUNC_FFMPEG_DIR "D:/android/workspaces/ffmpeg/output/android/arm64-v8a")
endif()

if(NOT EXISTS "${UNTRUNC_FFMPEG_DIR}/include/libavformat/avformat.h")
    message(FATAL_ERROR "FFmpeg headers not found under ${UNTRUNC_FFMPEG_DIR}/include (set -DUNTRUNC_FFMPEG_DIR=...)")
endif()

# engine sources (mirrors Makefile's wildcard src/*.cpp src/avc1/*.cpp src/hvc1/*.cpp,
# minus main.cpp which is the CLI entry)
file(GLOB UNTRUNC_SRC
     src/*.cpp
     src/avc1/*.cpp
     src/hvc1/*.cpp)
list(REMOVE_ITEM UNTRUNC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# JNI bridge lives next to this CMakeLists (android-lib module is self-contained)
list(APPEND UNTRUNC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/untrunc-jni.cpp")

add_library(untrunc SHARED ${UNTRUNC_SRC})

target_include_directories(untrunc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${UNTRUNC_FFMPEG_DIR}/include)

target_compile_definitions(untrunc PRIVATE
    _FILE_OFFSET_BITS=64
    UNTR_VERSION="android")

# exceptions are used throughout the engine (throw "literal")
target_compile_options(untrunc PRIVATE
    -fexceptions
    -frtti
    -Wno-unused-result)

# 16KB page-size device support: force max-page-size alignment on the
# ELF LOAD segments (Android 15+ devices with 16KB kernels).
target_link_options(untrunc PRIVATE
    "-Wl,-z,max-page-size=16384")

find_library(log-lib log)
find_library(android-lib android)

# import FFmpeg as prebuilt shared dependencies
add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION
    ${UNTRUNC_FFMPEG_DIR}/lib/libavformat.so)
add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION
    ${UNTRUNC_FFMPEG_DIR}/lib/libavcodec.so)
add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION
    ${UNTRUNC_FFMPEG_DIR}/lib/libavutil.so)

target_link_libraries(untrunc
    PRIVATE avformat avcodec avutil ${log-lib} ${android-lib})

# copy ffmpeg .so next to our output so they can be packaged together
add_custom_command(TARGET untrunc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${UNTRUNC_FFMPEG_DIR}/lib/libavformat.so
        ${UNTRUNC_FFMPEG_DIR}/lib/libavcodec.so
        ${UNTRUNC_FFMPEG_DIR}/lib/libavutil.so
        $<TARGET_FILE_DIR:untrunc>)