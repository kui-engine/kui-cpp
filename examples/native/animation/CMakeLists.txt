# animation examples
add_subdirectory(01_keyframes)
add_subdirectory(02_transition_timing)
add_subdirectory(03_lottie_player)
add_subdirectory(04_transition_all)
add_subdirectory(05_animation_delay)
add_subdirectory(06_animation_fill)
add_subdirectory(07_animation_iteration)
add_subdirectory(08_transform_animation)

add_custom_target(kui_native_lottie_asset ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:native_animation_03_lottie_player>/native_animation"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/pulse.json"
            "$<TARGET_FILE_DIR:native_animation_03_lottie_player>/native_animation/pulse.json"
    COMMENT "Copy native Lottie example asset"
)
add_dependencies(native_animation_03_lottie_player kui_native_lottie_asset)
