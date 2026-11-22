add_executable(native_extension_18_kui_editor_syntax_highlight main.cpp)
target_link_libraries(native_extension_18_kui_editor_syntax_highlight PRIVATE kui)

# Ensure kui_ext_editor.dll is built before this example.
if(TARGET kui_ext_editor)
    add_dependencies(native_extension_18_kui_editor_syntax_highlight kui_ext_editor)
endif()
