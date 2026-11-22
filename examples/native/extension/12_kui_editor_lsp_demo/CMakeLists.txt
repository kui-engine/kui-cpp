add_executable(native_extension_12_kui_editor_lsp_demo main.cpp)
target_link_libraries(native_extension_12_kui_editor_lsp_demo PRIVATE kui)

# Ensure kui_ext_editor.dll is built before this example.
if(TARGET kui_ext_editor)
    add_dependencies(native_extension_12_kui_editor_lsp_demo kui_ext_editor)
endif()
