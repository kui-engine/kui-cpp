add_executable(native_extension_17_kui_editor_minimap_scroll main.cpp)
target_link_libraries(native_extension_17_kui_editor_minimap_scroll PRIVATE kui)

# Ensure kui_ext_editor.dll is built before this example.
if(TARGET kui_ext_editor)
    add_dependencies(native_extension_17_kui_editor_minimap_scroll kui_ext_editor)
endif()
