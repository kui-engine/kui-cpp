add_executable(native_extension_11_kui_editor_m1_demo main.cpp)
target_link_libraries(native_extension_11_kui_editor_m1_demo PRIVATE kui)

# Ensure kui_ext_editor.dll is built before this example.
if(TARGET kui_ext_editor)
    add_dependencies(native_extension_11_kui_editor_m1_demo kui_ext_editor)
endif()
