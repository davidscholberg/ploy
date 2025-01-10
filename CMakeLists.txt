cmake_minimum_required(VERSION 3.16)

project(
    ploy
    VERSION 1.0.0
    DESCRIPTION "Scheme-like language interpreter for funsies"
)

add_executable(${PROJECT_NAME})

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if (GNU OR CLANG)
    target_compile_options(
        ${PROJECT_NAME}
        PUBLIC
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
elseif (MSVC)
    target_compile_options(
        ${PROJECT_NAME}
        PUBLIC
        /W4
        /WX
        # disable warnings from external headers
        /external:anglebrackets
        /external:W0
        /external:templates-
    )
endif ()

add_subdirectory(src)
