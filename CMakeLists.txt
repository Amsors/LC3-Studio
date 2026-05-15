cmake_minimum_required(VERSION 3.21)

project(lc3_studio_linux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FLTK_LINUX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fltk-linux")
set(LC3_EXAMPLES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/examples")
file(GLOB LC3_EXAMPLE_FILES CONFIGURE_DEPENDS "${LC3_EXAMPLES_DIR}/*.asm")
set(LC3_EMBEDDED_EXAMPLES_CPP "${CMAKE_BINARY_DIR}/generated/embedded_examples.cpp")

if(NOT EXISTS "${FLTK_LINUX_ROOT}/share/fltk/FLTKConfig.cmake")
    message(FATAL_ERROR
        "Missing prebuilt FLTK package at ${FLTK_LINUX_ROOT}.\n"
        "Expected an installed FLTK 1.4.x tree under third_party/fltk-linux.")
endif()

find_package(FLTK CONFIG REQUIRED
    PATHS "${FLTK_LINUX_ROOT}/share/fltk"
    NO_DEFAULT_PATH
)

add_custom_command(
    OUTPUT "${LC3_EMBEDDED_EXAMPLES_CPP}"
    COMMAND "${CMAKE_COMMAND}"
        "-DEXAMPLES_DIR=${LC3_EXAMPLES_DIR}"
        "-DOUTPUT=${LC3_EMBEDDED_EXAMPLES_CPP}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_examples.cmake"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_examples.cmake"
        ${LC3_EXAMPLE_FILES}
    COMMENT "Embedding LC-3 example programs"
    VERBATIM
)

set(LC3_STUDIO_SOURCE_FILES
    src/main.cpp
    src/app/command_line.cpp
    src/app/self_test.cpp
    src/gui/main_window.cpp
    src/gui/main_window_callbacks.cpp
    src/gui/main_window_commands.cpp
    src/gui/main_window_layout.cpp
    src/gui/main_window_refresh.cpp
    src/gui/io/file_utils.cpp
    src/gui/widgets/asm_highlighter.cpp
    src/gui/widgets/memory_table.cpp
    src/gui/widgets/register_table.cpp
    LC3/lc3_assembler_adapter.cpp
    LC3/lc3_simulator_adapter.cpp
)

set(LC3_STUDIO_SOURCES
    ${LC3_STUDIO_SOURCE_FILES}
    "${LC3_EMBEDDED_EXAMPLES_CPP}"
)

set(LC3_STUDIO_HEADERS
    src/app/command_line.h
    src/app/self_test.h
    src/examples/embedded_examples.h
    src/gui/main_window.h
    src/gui/main_window_internal.h
    src/gui/io/file_utils.h
    src/gui/widgets/asm_highlighter.h
    src/gui/widgets/cell_bounds.h
    src/gui/widgets/memory_table.h
    src/gui/widgets/register_table.h
    LC3/lc3_gui_adapter.h
)

add_executable(lc3_studio ${LC3_STUDIO_SOURCES} ${LC3_STUDIO_HEADERS})

target_include_directories(lc3_studio PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(lc3_studio PRIVATE fltk::images)

target_compile_options(lc3_studio PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

set_target_properties(lc3_studio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${LC3_STUDIO_SOURCE_FILES}
    ${LC3_STUDIO_HEADERS}
)
source_group("Generated" FILES "${LC3_EMBEDDED_EXAMPLES_CPP}")

message(STATUS "Configured Linux/CLion build for lc3_studio")
message(STATUS "Using prebuilt FLTK from ${FLTK_LINUX_ROOT}")
message(STATUS "Windows Visual Studio workflow remains in lc3_studio.sln / lc3_studio.vcxproj")
