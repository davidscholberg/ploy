target_sources(
    ${PROJECT_NAME}
    PRIVATE
    compiler.cpp
    compiler.hpp
    main.cpp
    opcodes.hpp
    tokenizer.cpp
    tokenizer.hpp
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
