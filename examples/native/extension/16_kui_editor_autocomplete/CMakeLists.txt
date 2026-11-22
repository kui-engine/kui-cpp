add_executable(native_extension_16_kui_editor_autocomplete main.cpp)
target_link_libraries(native_extension_16_kui_editor_autocomplete PRIVATE kui)

# Ensure kui_ext_editor.dll is built before this example.
if(TARGET kui_ext_editor)
    add_dependencies(native_extension_16_kui_editor_autocomplete kui_ext_editor)
endif()
