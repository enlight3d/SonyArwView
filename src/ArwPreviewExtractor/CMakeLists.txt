# ArwPreviewExtractor - shared static library
#
# Pure parsing logic + JPEG validation. Depends only on the Windows Imaging
# Component (windowscodecs) and core COM (ole32). No third-party dependencies.

add_library(ArwPreviewExtractor STATIC
    ArwPreviewExtractor.cpp
    ArwPreviewExtractor.h
    TiffReader.cpp
    TiffReader.h
    JpegValidation.cpp
    JpegValidation.h)

# Consumers include these headers via the directory.
target_include_directories(ArwPreviewExtractor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# WIC + COM. PUBLIC so executables/DLLs linking this lib inherit them.
target_link_libraries(ArwPreviewExtractor PUBLIC windowscodecs ole32 oleaut32)

arw_common_target(ArwPreviewExtractor)
