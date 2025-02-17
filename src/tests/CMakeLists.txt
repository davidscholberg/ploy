set(tests_target ${PROJECT_NAME}tests)

add_executable(${tests_target})

setup_project_target(${tests_target})

target_sources(
    ${tests_target}
    PRIVATE
    main.cpp
)

target_include_directories(
    ${tests_target}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ${tests_target}
    ${PROJECT_NAME}lib
)

include(CTest)

add_test(
    NAME ${tests_target}
    COMMAND ${tests_target}
)
