# ThumbTest - directly exercise the registered thumbnail provider COM object.

add_executable(ThumbTest main.cpp)

target_include_directories(ThumbTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/SonyArwThumbnailProvider)

target_link_libraries(ThumbTest PRIVATE
    windowscodecs ole32 oleaut32 shlwapi gdi32 uuid)

arw_common_target(ThumbTest)

if(MSVC)
    set_target_properties(ThumbTest PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()
