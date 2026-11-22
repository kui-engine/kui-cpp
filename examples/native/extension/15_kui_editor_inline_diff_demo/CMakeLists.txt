add_executable(native_extension_15_kui_editor_inline_diff_demo main.cpp)
target_link_libraries(native_extension_15_kui_editor_inline_diff_demo PRIVATE kui)

# Ensure kui_ext_editor.dll is built before this example.
if(TARGET kui_ext_editor)
    add_dependencies(native_extension_15_kui_editor_inline_diff_demo kui_ext_editor)
endif()
