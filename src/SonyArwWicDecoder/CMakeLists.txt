# SonyArwWicDecoder - WIC decoder COM in-proc server (DLL)

add_library(SonyArwWicDecoder SHARED
    dllmain.cpp
    guids.cpp
    guids.h
    DllCommon.h
    Debug.cpp
    Debug.h
    ClassFactory.cpp
    ClassFactory.h
    WicDecoder.cpp
    WicDecoder.h
    WicFrameDecode.cpp
    WicFrameDecode.h
    Registration.cpp
    Registration.h
    SonyArwWicDecoder.def)

target_link_libraries(SonyArwWicDecoder PRIVATE
    ArwPreviewExtractor
    windowscodecs
    ole32
    oleaut32
    shlwapi
    advapi32
    uuid)

arw_common_target(SonyArwWicDecoder)

# Keep the on-disk name exactly "SonyArwWicDecoder.dll".
set_target_properties(SonyArwWicDecoder PROPERTIES
    OUTPUT_NAME "SonyArwWicDecoder"
    PREFIX "")
