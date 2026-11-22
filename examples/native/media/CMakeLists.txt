# media examples
add_subdirectory(01_audio_player)
add_subdirectory(02_video_player)
add_subdirectory(03_media_controls)

add_custom_target(kui_native_media_assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:native_media_01_audio_player>/native_media"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${KUI_EXAMPLES_ROOT_DIR}/resources/audio/sample.mp3"
            "$<TARGET_FILE_DIR:native_media_01_audio_player>/native_media/sample.mp3"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${KUI_EXAMPLES_ROOT_DIR}/resources/video/sample.mp4"
            "$<TARGET_FILE_DIR:native_media_01_audio_player>/native_media/sample.mp4"
    COMMENT "Copy native media example assets"
)

foreach(_target IN ITEMS
        native_media_01_audio_player
        native_media_02_video_player
        native_media_03_media_controls)
    add_dependencies(${_target} kui_native_media_assets)
endforeach()
