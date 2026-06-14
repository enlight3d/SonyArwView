# SonyArwPreviewExtract - Phase 1 console tool

add_executable(SonyArwPreviewExtract main.cpp)

target_link_libraries(SonyArwPreviewExtract PRIVATE ArwPreviewExtractor)

arw_common_target(SonyArwPreviewExtract)

# Console subsystem with a Unicode entry point (wmain).
if(MSVC)
    set_target_properties(SonyArwPreviewExtract PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()
