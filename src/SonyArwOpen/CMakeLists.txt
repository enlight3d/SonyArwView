# SonyArwOpen - windowless .arw open handler bundled in the MSIX package.

add_executable(SonyArwOpen main.cpp)

target_link_libraries(SonyArwOpen PRIVATE ArwPreviewExtractor shell32 user32)

arw_common_target(SonyArwOpen)

# GUI subsystem (no console window) but keep wmain as the entry point.
if(MSVC)
    set_target_properties(SonyArwOpen PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
endif()
