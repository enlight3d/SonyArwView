# WicDecodeTest - WIC decode path tester

add_executable(WicDecodeTest main.cpp)

# Needs our decoder's GUID declarations (CLSID_SonyArwDecoder).
target_include_directories(WicDecodeTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/SonyArwWicDecoder)

target_link_libraries(WicDecodeTest PRIVATE
    windowscodecs ole32 oleaut32 shlwapi uuid)

arw_common_target(WicDecodeTest)

if(MSVC)
    set_target_properties(WicDecodeTest PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()
