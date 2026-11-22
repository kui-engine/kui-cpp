# extension examples
add_subdirectory(01_hello_extension)
add_subdirectory(02_custom_tag)
add_subdirectory(03_style_injection)
add_subdirectory(04_dom_observer)
add_subdirectory(05_toolbar)
add_subdirectory(06_notification)
add_subdirectory(07_data_table)
add_subdirectory(08_chart_widget)
add_subdirectory(09_spectrum_visualizer)
add_subdirectory(10_kui_editor)
add_subdirectory(11_kui_editor_m1_demo)
add_subdirectory(12_kui_editor_lsp_demo)
add_subdirectory(13_kui_editor_m3_demo)
add_subdirectory(14_kui_editor_large_file_demo)
add_subdirectory(15_kui_editor_inline_diff_demo)
add_subdirectory(16_kui_editor_autocomplete)
add_subdirectory(17_kui_editor_minimap_scroll)
add_subdirectory(18_kui_editor_syntax_highlight)

foreach(_target IN ITEMS
        native_extension_01_hello_extension
        native_extension_02_custom_tag
        native_extension_03_style_injection
        native_extension_04_dom_observer
        native_extension_05_toolbar
        native_extension_06_notification
        native_extension_07_data_table
        native_extension_08_chart_widget
        native_extension_09_spectrum_visualizer)
    add_dependencies(${_target} kui_ext_hello)
endforeach()
