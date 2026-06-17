# SonyArwOpen - .arw open handler + settings window, bundled in the MSIX package.
# With a file argument it opens the photo; with none (Start menu) it shows the
# settings dialog (Settings.cpp / Settings.rc).

add_executable(SonyArwOpen main.cpp Settings.cpp Settings.rc)

target_link_libraries(SonyArwOpen PRIVATE ArwPreviewExtractor shell32 user32 advapi32 comdlg32 comctl32)

arw_common_target(SonyArwOpen)

# Compile sources as UTF-8 so the accented French string literals in Loc.h get the
# correct wide-char codepoints.
if(MSVC)
    target_compile_options(SonyArwOpen PRIVATE /utf-8)
endif()

# Windows subsystem: no console window -> no command-window flash when opening an
# .arw. The opened file arrives as a normal command-line argument (the manifest
# declares the association with Parameters="%1", which a full-trust packaged app
# receives on argv), so no window or message loop is needed -- the process reads
# argv[1], does its work, and exits.
#
# /MANIFEST:NO: we embed our own application manifest via Settings.rc (it pulls in
# Common Controls v6 for modern Visual Styles); disable the linker's auto-generated
# one so there's exactly one manifest in the exe.
if(MSVC)
    set_target_properties(SonyArwOpen PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /MANIFEST:NO")
endif()
