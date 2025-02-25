include(CTest)

add_test(
    NAME ${PROJECT_NAME}tests
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/test_cases
)
