add_executable(native_3d_engine_17_model_loading main.cpp)
target_link_libraries(native_3d_engine_17_model_loading PRIVATE kui)
add_custom_command(TARGET native_3d_engine_17_model_loading POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:native_3d_engine_17_model_loading>/native_3d"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${KUI_EXAMPLES_ROOT_DIR}/resources/3d models/stylized_tank.glb"
        "$<TARGET_FILE_DIR:native_3d_engine_17_model_loading>/native_3d/stylized_tank.glb"
    COMMENT "[native_3d_engine_17_model_loading] Copying bundled textured GLB"
    VERBATIM)
