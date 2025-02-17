set(lib_target ${PROJECT_NAME}lib)

add_library(${lib_target})

setup_project_target(${lib_target})

target_sources(
    ${lib_target}
    PRIVATE
    bytecode.cpp
    compiler.cpp
    include/bytecode.hpp
    include/compiler.hpp
    include/scheme_value.hpp
    include/template_appender.hpp
    include/tokenizer.hpp
    include/virtual_machine.hpp
    overload.hpp
    tokenizer.cpp
    virtual_machine.cpp
)

target_include_directories(
    ${lib_target}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
