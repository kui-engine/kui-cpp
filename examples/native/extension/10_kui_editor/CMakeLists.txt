add_executable(native_extension_10_kui_editor main.cpp)
target_link_libraries(native_extension_10_kui_editor PRIVATE kui)

# Ensure kui_ext_editor.dll is built before this example, so that running it
# straight from the project root will find the extension in build/extensions/.
if(TARGET kui_ext_editor)
    add_dependencies(native_extension_10_kui_editor kui_ext_editor)
endif()
