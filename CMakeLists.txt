# ==============================================================================
# kui_cpp — C++ wrapper over the KUI C ABI, delivered as SOURCE
#
# There is no binary target here on purpose. kui_cpp is an INTERFACE library:
# the header compiles into YOUR translation units with YOUR compiler and YOUR
# CRT, and the only binary boundary in the chain is kui_c.dll's pure C ABI.
#
# Shipping a prebuilt kui_cpp.lib would put a C++ ABI back in the middle and
# undo the entire point -- the same reason CEF ships libcef_dll_wrapper as
# source only. Do not add a compiled target here.
#
# Usage from a consumer project (the C++ Wrapper SDK package also offers
# find_package(KuiC) + KuiC::Cpp, see README.md):
#     set(KUI_C_ROOT "<Wrapper package root>" CACHE PATH "")
#     add_subdirectory(<Wrapper package root>/cpp_wrapper kui_cpp)
#     target_link_libraries(my_app PRIVATE kui_cpp)
# ==============================================================================
cmake_minimum_required(VERSION 3.20)
project(kui_cpp LANGUAGES CXX)

# Locate the C ABI: in tree the kui_c target already exists; standalone, point
# KUI_C_ROOT at the C++ Wrapper SDK package root (it carries kui_c.h, the KuiC
# config and the runtime) or, in the repository, at sdk/c with a built kui_c.
set(KUI_C_ROOT "" CACHE PATH "Root of the KUI C ABI (Wrapper package root, or sdk/c with a built kui_c)")
option(KUI_CPP_BUILD_NATIVE_COMPAT_EXAMPLES
       "Build the repository's native examples against the C-ABI C++ facade" OFF)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/native/CMakeLists.txt")
    # Release packages carry a source snapshot here, so the native examples (and C++ API tours) remain
    # buildable after extraction without access to the KUI repository.
    set(_KUI_CPP_DEFAULT_EXAMPLES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/examples")
else()
    # In-tree development deliberately reuses the canonical repository sources.
    set(_KUI_CPP_DEFAULT_EXAMPLES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../examples")
endif()
set(KUI_CPP_NATIVE_EXAMPLES_ROOT "${_KUI_CPP_DEFAULT_EXAMPLES_ROOT}"
    CACHE PATH "Root of the KUI native examples tree")

add_library(kui_cpp INTERFACE)
target_include_directories(kui_cpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# The wrapper needs C++17 for [[nodiscard]] and if-constexpr-free niceties; it
# does NOT need to match whatever standard kui.dll was built with, because no C++
# type crosses the boundary. That independence is the whole feature.
target_compile_features(kui_cpp INTERFACE cxx_std_17)
target_compile_definitions(kui_cpp INTERFACE
    KUI_CPP_WRAPPER=1
    KUI_HAS_SCRIPTING=1
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

if(TARGET kui_c)
    target_link_libraries(kui_cpp INTERFACE kui_c)
    message(STATUS "kui_cpp: using in-tree kui_c target")
elseif(KUI_C_ROOT AND EXISTS "${KUI_C_ROOT}/cmake/KuiCConfig.cmake")
    set(KuiC_DIR "${KUI_C_ROOT}/cmake")
    find_package(KuiC CONFIG REQUIRED)
    target_link_libraries(kui_cpp INTERFACE KuiC::C)
    message(STATUS "kui_cpp: using packaged KuiC targets at ${KUI_C_ROOT}")
elseif(KUI_C_ROOT)
    target_include_directories(kui_cpp INTERFACE "${KUI_C_ROOT}/include")
    # Search every layout the C library can land in: a packaged SDK puts it in
    # lib/, a multi-config generator (Visual Studio) in build/<Config>/, and a
    # single-config generator (Makefiles/Ninja on macOS and Linux) directly in
    # build/. find_library supplies the platform's own prefix/suffix, so `kui_c`
    # matches kui_c.lib, libkui_c.dylib and libkui_c.so alike.
    find_library(KUI_C_LIBRARY NAMES kui_c
                 HINTS "${KUI_C_ROOT}/lib"
                       "${KUI_C_ROOT}/build/Release"
                       "${KUI_C_ROOT}/build/Debug"
                       "${KUI_C_ROOT}/build"
                       "${KUI_C_ROOT}/build-verify/Release"
                       "${KUI_C_ROOT}/build-verify"
                 REQUIRED)
    target_link_libraries(kui_cpp INTERFACE "${KUI_C_LIBRARY}")
    message(STATUS "kui_cpp: using C SDK at ${KUI_C_ROOT} (${KUI_C_LIBRARY})")
else()
    message(FATAL_ERROR
        "kui_cpp needs the C ABI. Either add_subdirectory the C SDK first so the "
        "kui_c target exists, or pass -DKUI_C_ROOT=<Wrapper package root, or sdk/c>.")
endif()

# --- Demo / verification ------------------------------------------------------
# Built only when this project is configured directly, so a consumer that
# add_subdirectory's the wrapper does not inherit our examples.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(kui_cpp_demo examples/wrapper_demo.cpp)
    target_link_libraries(kui_cpp_demo PRIVATE kui_cpp)
    if(MSVC)
        target_compile_options(kui_cpp_demo PRIVATE /utf-8 /W4)
    endif()

    add_executable(test_kui_cpp tests/test_kui_cpp.cpp)
    target_link_libraries(test_kui_cpp PRIVATE kui_cpp)
    if(MSVC)
        target_compile_options(test_kui_cpp PRIVATE /utf-8 /W4)
    endif()

    add_executable(test_kui_cpp_compat tests/test_kui_compat.cpp)
    target_link_libraries(test_kui_cpp_compat PRIVATE kui_cpp)
    if(MSVC)
        target_compile_options(test_kui_cpp_compat PRIVATE /utf-8 /W4)
    endif()

    add_executable(test_kui_cpp_full tests/test_kui_full.cpp)
    target_link_libraries(test_kui_cpp_full PRIVATE kui_cpp)
    if(MSVC)
        target_compile_options(test_kui_cpp_full PRIVATE /utf-8 /W4)
    endif()

    set(_kui_cpp_runtime_targets
        kui_cpp_demo test_kui_cpp test_kui_cpp_compat test_kui_cpp_full)
    if(KUI_CPP_BUILD_NATIVE_COMPAT_EXAMPLES)
        include("${CMAKE_CURRENT_SOURCE_DIR}/examples/native_compat.cmake")
        list(APPEND _kui_cpp_runtime_targets ${KUI_CPP_NATIVE_COMPAT_TARGETS})
    endif()

    # Windows has no rpath: the DLLs must sit next to the executable. Elsewhere
    # CMake's build rpath already points at the shared libraries, so copying is
    # unnecessary (and the .dll paths below simply do not exist there -- an
    # earlier version of this file hardcoded them and broke the macOS configure).
    #
    # Only files that actually exist are copied, since which of them are present
    # depends on how the C SDK was built.
    if(WIN32)
        if(COMMAND kui_c_copy_runtime_dlls)
            foreach(_tgt IN LISTS _kui_cpp_runtime_targets)
                kui_c_copy_runtime_dlls(${_tgt})
            endforeach()
        else()
            set(_kui_runtime "")
            if(TARGET kui_c)
                list(APPEND _kui_runtime $<TARGET_FILE:kui_c>)
            elseif(KUI_C_LIBRARY)
                get_filename_component(_kui_c_dir "${KUI_C_LIBRARY}" DIRECTORY)
                foreach(_dll "kui_c.dll" "kui.dll")
                    if(EXISTS "${_kui_c_dir}/${_dll}")
                        list(APPEND _kui_runtime "${_kui_c_dir}/${_dll}")
                    endif()
                endforeach()
            endif()
            # 这些目标全部使用同一个默认输出目录，所以过去"每个目标各挂一条
            # POST_BUILD copy"会让并行构建同时把同一个 DLL 写到同一个目的文件：
            # -j16 下实测 cmake -E copy_if_different 报 "Error copying file"
            # 并使整条构建以 MSB3073 失败。改为只在第一个目标上拷贝一次，其余
            # 目标依赖它，从而既保证 DLL 在 exe 旁边，又不会并发写同一个文件。
            if(_kui_runtime)
                list(GET _kui_cpp_runtime_targets 0 _kui_cpp_runtime_anchor)
                foreach(_rt IN LISTS _kui_runtime)
                    add_custom_command(TARGET ${_kui_cpp_runtime_anchor} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_rt}" $<TARGET_FILE_DIR:${_kui_cpp_runtime_anchor}>
                        VERBATIM)
                endforeach()
                foreach(_tgt IN LISTS _kui_cpp_runtime_targets)
                    if(NOT _tgt STREQUAL _kui_cpp_runtime_anchor)
                        add_dependencies(${_tgt} ${_kui_cpp_runtime_anchor})
                    endif()
                endforeach()
            endif()
        endif()
    endif()

    enable_testing()
    add_test(NAME kui_cpp COMMAND test_kui_cpp)
    add_test(NAME kui_cpp_compat COMMAND test_kui_cpp_compat)
    add_test(NAME kui_cpp_full COMMAND test_kui_cpp_full)
endif()
