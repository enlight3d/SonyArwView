# SonyARW - embedded-preview WIC codec toolkit
#
# Top-level build. Targets are added phase by phase:
#   Phase 1: ArwPreviewExtractor (static lib) + SonyArwPreviewExtract (console)
#   Phase 2: SonyArwWicDecoder (DLL) + WicDecodeTest (console)   [added later]
#   Phase 3: SonyArwThumbnailProvider (DLL)                       [optional]
#
# Build (Visual Studio 2022 / BuildTools, x64):
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
#   cmake --build build --config Release
#
# Build (Ninja from a "x64 Native Tools" / vcvars64 prompt):
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build

cmake_minimum_required(VERSION 3.20)

project(SonyArwTools
    VERSION 0.1.0
    DESCRIPTION "Sony .ARW embedded-JPEG-preview extractor + WIC codec"
    LANGUAGES CXX)

# ---- Global standards ------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link the C/C++ runtime statically so the WIC/shell DLLs and console tools have
# NO dependency on the VC++ redistributable. This matters for COM servers loaded
# by Explorer/Photos on machines that may not have the redist installed.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Always build 64-bit components here; the WIC/shell DLLs must match the host
# process bitness (Explorer/Photos are x64 on Windows 11 x64).
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "This project targets x64. Configure with -A x64 (VS) or a x64 toolchain.")
endif()

# Helper: apply common compile options / definitions to a target.
function(arw_common_target tgt)
    target_compile_definitions(${tgt} PRIVATE
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN NOMINMAX
        _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /EHsc /permissive-)
    else()
        # MinGW / Clang fallback (the COM DLLs are tuned for MSVC, but the
        # extractor + console tool build cleanly with GCC too).
        target_compile_options(${tgt} PRIVATE -Wall -Wextra)
    endif()
endfunction()

add_subdirectory(src/ArwPreviewExtractor)
add_subdirectory(src/SonyArwPreviewExtract)

# Phase 2: WIC decoder DLL + its test harness.
add_subdirectory(src/SonyArwWicDecoder)
add_subdirectory(src/WicDecodeTest)

# Phase 3: dedicated shell thumbnail provider (Explorer thumbnails). Empirically
# required because WIC registration alone does not drive Explorer thumbnails for
# .arw (the extension is owned by a registered IThumbnailProvider).
add_subdirectory(src/SonyArwThumbnailProvider)
add_subdirectory(src/ThumbTest)
