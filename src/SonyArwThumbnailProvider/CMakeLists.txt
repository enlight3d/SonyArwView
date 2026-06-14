# SonyArwThumbnailProvider - shell thumbnail handler COM in-proc server (DLL)

add_library(SonyArwThumbnailProvider SHARED
    dllmain.cpp
    guids.cpp
    guids.h
    DllCommon.h
    Debug.cpp
    Debug.h
    ClassFactory.cpp
    ClassFactory.h
    ThumbnailProvider.cpp
    ThumbnailProvider.h
    Registration.cpp
    Registration.h
    SonyArwThumbnailProvider.def)

target_link_libraries(SonyArwThumbnailProvider PRIVATE
    ArwPreviewExtractor
    windowscodecs
    ole32
    oleaut32
    shlwapi
    advapi32
    gdi32
    shell32
    uuid)

arw_common_target(SonyArwThumbnailProvider)

set_target_properties(SonyArwThumbnailProvider PROPERTIES
    OUTPUT_NAME "SonyArwThumbnailProvider"
    PREFIX "")
