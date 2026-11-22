# canvas2d examples
add_subdirectory(01_basic_drawing)
add_subdirectory(02_image_painting)
add_subdirectory(03_chart_renderer)
add_subdirectory(04_animation)
add_subdirectory(05_path_drawing)

set(_kui_native_canvas_targets
    native_canvas2d_01_basic_drawing
    native_canvas2d_02_image_painting
    native_canvas2d_03_chart_renderer
    native_canvas2d_04_animation
    native_canvas2d_05_path_drawing
)
add_custom_target(kui_native_canvas_assets ALL
    COMMENT "[KUI] Copying functional native Canvas 2D sample")
foreach(_target IN LISTS _kui_native_canvas_targets)
    add_custom_command(TARGET kui_native_canvas_assets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${_target}>/native_samples"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${KUI_EXAMPLES_ROOT_DIR}/examples/html_loader/samples/44_canvas_2d_api.html"
            "$<TARGET_FILE_DIR:${_target}>/native_samples/44_canvas_2d_api.html"
        COMMENT "[${_target}] Copying functional Canvas 2D API sample"
        VERBATIM)
    add_dependencies(${_target} kui_native_canvas_assets)
endforeach()
