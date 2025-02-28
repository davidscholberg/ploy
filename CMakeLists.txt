cmake_minimum_required(VERSION 3.16)

project(
    ploy
    VERSION 0.0.1
    DESCRIPTION "Scheme interpreter implemented as a stack-based bytecode virtual machine and written in C++23"
)

function(setup_project_target target)
    set_target_properties(
        ${target}
        PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    if (GNU OR CLANG)
        target_compile_options(
            ${target}
            PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    elseif (MSVC)
        target_compile_options(
            ${target}
            PRIVATE
            /W4
            /WX
            # disable warnings from external headers
            /external:anglebrackets
            /external:W0
            /external:templates-
        )
    endif()
endfunction()

add_subdirectory(src)
