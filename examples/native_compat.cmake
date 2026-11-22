# All native examples supported by the Windows Full Profile C ABI facade. In the
# repository these paths resolve to the canonical examples/native tree; release
# packages contain a source snapshot under cpp_wrapper/examples/native so the
# exact same 189 targets remain independently buildable after extraction.
set(_kui_cpp_native_compat_sources
    native/html/01_box_model/main.cpp
    native/html/02_flexbox/main.cpp
    native/html/03_grid_layout/main.cpp
    native/html/04_css_styling/main.cpp
    native/html/05_typography/main.cpp
    native/html/06_images/main.cpp
    native/html/07_transitions/main.cpp
    native/html/08_timer_animation/main.cpp
    native/html/09_forms/main.cpp
    native/html/10_positioning/main.cpp
    native/html/11_gradients/main.cpp
    native/html/12_shadows/main.cpp
    native/html/13_pseudo_elements/main.cpp
    native/html/14_hover_states/main.cpp
    native/html/15_scroll/main.cpp
    native/html/16_css_variables/main.cpp
    native/html/17_tables/main.cpp
    native/html/18_lists/main.cpp
    native/html/19_transforms/main.cpp
    native/html/20_overflow/main.cpp
    native/html/21_html_loader/main.cpp

    native/advanced/01_dom_manipulation/main.cpp
    native/advanced/02_event_handling/main.cpp
    native/advanced/03_class_toggling/main.cpp
    native/advanced/06_theme_switching/main.cpp
    native/advanced/08_inner_html/main.cpp
    native/advanced/09_querySelector/main.cpp
    native/advanced/10_data_attributes/main.cpp

    native/component/01_tabs/main.cpp
    native/component/02_accordion/main.cpp
    native/component/03_modal/main.cpp
    native/component/04_tooltip_comp/main.cpp
    native/component/05_breadcrumb/main.cpp
    native/component/06_pagination/main.cpp
    native/component/07_progress_bar/main.cpp
    native/component/08_tag_input/main.cpp
    native/component/09_tree_view/main.cpp
    native/component/10_list_view/main.cpp

    native/layout/01_block_flow/main.cpp
    native/layout/02_inline_flow/main.cpp
    native/layout/03_float_clear/main.cpp
    native/layout/04_multi_column/main.cpp
    native/layout/05_aspect_ratio/main.cpp
    native/layout/06_min_max_sizes/main.cpp
    native/layout/07_calc_values/main.cpp
    native/layout/08_percentage_sizing/main.cpp
    native/layout/09_auto_margins/main.cpp
    native/layout/10_writing_mode/main.cpp
    native/layout/11_direction_rtl/main.cpp
    native/layout/12_nested_flex_grid/main.cpp

    native/effects/01_css_filters/main.cpp
    native/effects/02_blend_modes/main.cpp
    native/effects/03_backdrop_filter/main.cpp
    native/effects/04_clip_path/main.cpp
    native/effects/05_opacity_layers/main.cpp
    native/effects/06_border_image/main.cpp
    native/effects/07_outline/main.cpp
    native/effects/08_background_blend/main.cpp

    native/animation/01_keyframes/main.cpp
    native/animation/02_transition_timing/main.cpp
    native/animation/04_transition_all/main.cpp
    native/animation/05_animation_delay/main.cpp
    native/animation/06_animation_fill/main.cpp
    native/animation/07_animation_iteration/main.cpp
    native/animation/08_transform_animation/main.cpp

    native/styling/01_border_radius/main.cpp
    native/styling/02_font_stacks/main.cpp
    native/styling/03_color_formats/main.cpp
    native/styling/04_cursor_styles/main.cpp
    native/styling/05_visibility/main.cpp
    native/styling/06_z_index/main.cpp
    native/styling/07_word_wrap/main.cpp
    native/styling/08_counter_css/main.cpp
    native/styling/09_media_queries/main.cpp
    native/styling/10_supports_rule/main.cpp
    native/styling/11_import_rule/main.cpp
    native/styling/12_scoped_styles/main.cpp

    native/showcase/01_dashboard/main.cpp
    native/showcase/02_chat_app/main.cpp
    native/showcase/03_file_browser/main.cpp
    native/showcase/04_settings_page/main.cpp
    native/showcase/05_login_form/main.cpp
    native/showcase/06_kanban_board/main.cpp
    native/showcase/07_music_player/main.cpp
    native/showcase/08_game_ui/main.cpp

    native/a11y/01_semantic_html/main.cpp
    native/a11y/02_aria_roles/main.cpp
    native/a11y/03_focus_management/main.cpp
    native/a11y/04_keyboard_nav/main.cpp

    native/frameless/01_basic_frameless/main.cpp
    native/frameless/02_transparent_window/main.cpp
    native/frameless/03_always_on_top/main.cpp

    native/multiwindow/01_two_windows/main.cpp
    native/multiwindow/02_parent_child/main.cpp
    native/multiwindow/03_window_messaging/main.cpp
    native/multiwindow/04_window_positions/main.cpp

    native/platform/01_window_resize/main.cpp
    native/platform/02_dpi_scaling/main.cpp
    native/platform/06_timer_intervals/main.cpp

    native/devtools/01_console_log/main.cpp
    native/devtools/02_inspector/main.cpp
    native/devtools/03_hot_reload/main.cpp
    native/devtools/04_element_highlight/main.cpp
    native/devtools/05_performance/main.cpp
    native/devtools/06_network_panel/main.cpp

    native/3d_engine/01_hello_triangle/main.cpp
    native/3d_engine/02_rotating_cube/main.cpp
    native/3d_engine/03_lighting/main.cpp
    native/3d_engine/04_texture_mapping/main.cpp
    native/3d_engine/05_camera_orbit/main.cpp
    native/3d_engine/06_multiple_objects/main.cpp
    native/3d_engine/07_skybox/main.cpp
    native/3d_engine/08_shadows/main.cpp
    native/3d_engine/09_particles/main.cpp
    native/3d_engine/10_physics_basic/main.cpp
    native/3d_engine/11_physics_rigid_body/main.cpp
    native/3d_engine/12_physics_collision/main.cpp
    native/3d_engine/13_ecs_basic/main.cpp
    native/3d_engine/14_ecs_systems/main.cpp
    native/3d_engine/15_ecs_components/main.cpp
    native/3d_engine/16_scene_graph/main.cpp
    native/3d_engine/17_model_loading/main.cpp
    native/3d_engine/18_animation_skeletal/main.cpp
    native/3d_engine/19_post_processing/main.cpp
    native/3d_engine/20_pbr_materials/main.cpp
    native/3d_engine/21_terrain/main.cpp
    native/3d_engine/22_instancing/main.cpp
    native/3d_engine/23_html_3d_overlay/main.cpp
    native/3d_engine/24_raycast/main.cpp
    native/3d_engine/25_constraints/main.cpp
    native/3d_engine/26_debug_draw/main.cpp

    native/advanced/04_custom_element/main.cpp
    native/advanced/05_virtual_list/main.cpp
    native/advanced/07_computed_styles/main.cpp
    native/animation/03_lottie_player/main.cpp
    native/canvas2d/01_basic_drawing/main.cpp
    native/canvas2d/02_image_painting/main.cpp
    native/canvas2d/03_chart_renderer/main.cpp
    native/canvas2d/04_animation/main.cpp
    native/canvas2d/05_path_drawing/main.cpp
    native/component/11_anchored_popup/main.cpp
    native/component/12_tooltip_and_tabindex/main.cpp
    native/devtools/07_multi_target_discovery/main.cpp

    native/extension/01_hello_extension/main.cpp
    native/extension/02_custom_tag/main.cpp
    native/extension/03_style_injection/main.cpp
    native/extension/04_dom_observer/main.cpp
    native/extension/05_toolbar/main.cpp
    native/extension/06_notification/main.cpp
    native/extension/07_data_table/main.cpp
    native/extension/08_chart_widget/main.cpp
    native/extension/09_spectrum_visualizer/main.cpp
    native/extension/10_kui_editor/main.cpp
    native/extension/11_kui_editor_m1_demo/main.cpp
    native/extension/12_kui_editor_lsp_demo/main.cpp
    native/extension/13_kui_editor_m3_demo/main.cpp
    native/extension/14_kui_editor_large_file_demo/main.cpp
    native/extension/15_kui_editor_inline_diff_demo/main.cpp
    native/extension/16_kui_editor_autocomplete/main.cpp
    native/extension/17_kui_editor_minimap_scroll/main.cpp
    native/extension/18_kui_editor_syntax_highlight/main.cpp

    native/frameless/04_round_corners/main.cpp
    native/html/22_input_pickers/main.cpp
    native/html/23_print_pdf/main.cpp
    native/html/24_web_fonts/main.cpp
    native/media/01_audio_player/main.cpp
    native/media/02_video_player/main.cpp
    native/media/03_media_controls/main.cpp
    native/network/01_http_fetch/main.cpp
    native/network/02_async_request/main.cpp
    native/platform/03_clipboard/main.cpp
    native/platform/04_file_dialog/main.cpp
    native/platform/05_system_info/main.cpp
    native/platform/07_window_events/main.cpp
    native/platform/08_file_drag_drop/main.cpp
    native/script/01_lua_hello/main.cpp
    native/script/02_lua_dom/main.cpp
    native/script/03_lua_events/main.cpp
    native/script/04_lua_animation/main.cpp
    native/script/05_lua_data/main.cpp
    native/webgl/01_triangle/main.cpp
    native/webgl/02_textured_quad/main.cpp
    native/webgl/03_rotating_cube/main.cpp
    native/webgl/04_instanced_draw/main.cpp
    native/webgl/05_framebuffer/main.cpp
    native/webgl/06_shader_uniform/main.cpp
    native/webgl/07_gl_commands/main.cpp
    native/webgl/08_shader_edit/main.cpp
    native/webgl/09_animated_scene/main.cpp
)

set(KUI_CPP_NATIVE_COMPAT_TARGETS "")
# native/<group>/<name>/main.cpp -> target kui_cpp_<group>_<name>.
function(_kui_cpp_add_native_compat_example _source)
    set(_absolute "${KUI_CPP_NATIVE_EXAMPLES_ROOT}/${_source}")
    if(NOT EXISTS "${_absolute}")
        message(FATAL_ERROR "KUI wrapper example source is missing: ${_absolute}")
    endif()

    string(REGEX REPLACE "^native/" "" _name "${_source}")
    string(REGEX REPLACE "/main\\.cpp$" "" _name "${_name}")
    string(REPLACE "/" "_" _name "${_name}")
    set(_target "kui_cpp_${_name}")

    add_executable(${_target} "${_absolute}")
    target_link_libraries(${_target} PRIVATE kui_cpp)
    # The shared example sources use C++20 designated initializers for
    # WindowConfig even though the wrapper itself remains C++17-compatible.
    target_compile_features(${_target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /utf-8 /W4)
    endif()
    set_target_properties(${_target} PROPERTIES FOLDER "KUI C++ Wrapper Examples")
    set(KUI_CPP_NATIVE_COMPAT_TARGETS ${KUI_CPP_NATIVE_COMPAT_TARGETS} ${_target} PARENT_SCOPE)
endfunction()

foreach(_source IN LISTS _kui_cpp_native_compat_sources)
    _kui_cpp_add_native_compat_example("${_source}")
endforeach()

list(LENGTH KUI_CPP_NATIVE_COMPAT_TARGETS _compat_count)
if(NOT _compat_count EQUAL 189)
    message(FATAL_ERROR "kui_cpp Full Profile expects exactly 189 native examples, got ${_compat_count}")
endif()
message(STATUS "kui_cpp: ${_compat_count} native example sources use the C ABI facade")

# >>> API tours: generated by scripts/generate_cpp_tours.py from examples/api_tours; do not hand edit.
set(_kui_cpp_api_tour_sources
    native/api/data_observers/main.cpp
    native/api/dom_tree/main.cpp
    native/api/events/main.cpp
    native/api/extension_3d/main.cpp
    native/api/geometry_focus/main.cpp
    native/api/media_lottie/main.cpp
    native/api/network_devtools/main.cpp
    native/api/paint/main.cpp
    native/api/platform/main.cpp
    native/api/popups_input/main.cpp
    native/api/script_webgl/main.cpp
    native/api/style_class/main.cpp
    native/api/widgets/main.cpp
    native/api/window/main.cpp
)
foreach(_source IN LISTS _kui_cpp_api_tour_sources)
    _kui_cpp_add_native_compat_example("${_source}")
endforeach()
list(LENGTH _kui_cpp_api_tour_sources _tour_count)
message(STATUS "kui_cpp: ${_tour_count} API tour examples (native/api)")
# <<< API tours
