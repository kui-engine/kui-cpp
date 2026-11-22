add_executable(native_3d_engine_04_texture_mapping main.cpp)
target_link_libraries(native_3d_engine_04_texture_mapping PRIVATE kui)
add_custom_command(TARGET native_3d_engine_04_texture_mapping POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:native_3d_engine_04_texture_mapping>/native_3d"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${KUI_EXAMPLES_ROOT_DIR}/resources/3d models/stylized_tank.glb"
        "$<TARGET_FILE_DIR:native_3d_engine_04_texture_mapping>/native_3d/stylized_tank.glb"
    COMMENT "[native_3d_engine_04_texture_mapping] Copying bundled textured GLB"
    VERBATIM)
