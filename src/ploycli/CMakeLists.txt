add_executable(${PROJECT_NAME})

setup_project_target(${PROJECT_NAME})

target_sources(
    ${PROJECT_NAME}
    PRIVATE
    main.cpp
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ${PROJECT_NAME}
    ${PROJECT_NAME}lib
)
