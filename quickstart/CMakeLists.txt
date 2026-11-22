cmake_minimum_required(VERSION 3.20)
project(hello_kui LANGUAGES CXX)

find_package(KuiC 3 CONFIG REQUIRED)

add_executable(hello_kui main.cpp)
target_link_libraries(hello_kui PRIVATE KuiC::Cpp)
kui_c_copy_runtime_dlls(hello_kui)

# Put the page next to the program after every build.
add_custom_command(TARGET hello_kui POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/app.html" "$<TARGET_FILE_DIR:hello_kui>"
    VERBATIM)
