# SonyArwOpen - windowless .arw open handler bundled in the MSIX package.

add_executable(SonyArwOpen main.cpp)

target_link_libraries(SonyArwOpen PRIVATE ArwPreviewExtractor shell32 user32 advapi32)

arw_common_target(SonyArwOpen)

# Windows subsystem: no console window -> no command-window flash when opening an
# .arw. The opened file arrives as a normal command-line argument (the manifest
# declares the association with Parameters="%1", which a full-trust packaged app
# receives on argv), so no window or message loop is needed -- the process reads
# argv[1], does its work, and exits.
if(MSVC)
    set_target_properties(SonyArwOpen PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif()
