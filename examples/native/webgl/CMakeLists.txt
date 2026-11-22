# webgl examples
add_subdirectory(01_triangle)
add_subdirectory(02_textured_quad)
add_subdirectory(03_rotating_cube)
add_subdirectory(04_instanced_draw)
add_subdirectory(05_framebuffer)
add_subdirectory(06_shader_uniform)
add_subdirectory(07_gl_commands)
add_subdirectory(08_shader_edit)
add_subdirectory(09_animated_scene)

# 每个原生 WebGL 示例运行一份真实的 Lua/WebGL 场景。资源复制到可执行文件
# 旁边，避免把源码树绝对路径烘焙进 SDK consumer 的二进制。
set(_kui_native_webgl_samples
    "native_webgl_01_triangle|60_webgl_triangle.html"
    "native_webgl_02_textured_quad|native_02_textured_quad.html"
    "native_webgl_03_rotating_cube|61_webgl_cube.html"
    "native_webgl_04_instanced_draw|native_04_instanced_draw.html"
    "native_webgl_05_framebuffer|native_05_framebuffer.html"
    "native_webgl_06_shader_uniform|64_webgl_wave.html"
    "native_webgl_07_gl_commands|63_webgl_blending.html"
    "native_webgl_08_shader_edit|native_08_shader_edit.html"
    "native_webgl_09_animated_scene|69_webgl_galaxy_shader.html"
)

add_custom_target(kui_native_webgl_assets ALL
    COMMENT "[KUI] Copying functional native WebGL samples")
foreach(_spec IN LISTS _kui_native_webgl_samples)
    string(REPLACE "|" ";" _parts "${_spec}")
    list(GET _parts 0 _target)
    list(GET _parts 1 _sample)
    if(_sample MATCHES "^native_")
        set(_source "${CMAKE_CURRENT_SOURCE_DIR}/assets/${_sample}")
    else()
        set(_source "${KUI_EXAMPLES_ROOT_DIR}/examples/html_loader/samples/${_sample}")
    endif()
    add_custom_command(TARGET kui_native_webgl_assets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${_target}>/native_samples"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_source}"
            "$<TARGET_FILE_DIR:${_target}>/native_samples/${_sample}"
        COMMENT "[${_target}] Copying functional WebGL sample ${_sample}"
        VERBATIM)
    add_dependencies(${_target} kui_native_webgl_assets)
endforeach()
