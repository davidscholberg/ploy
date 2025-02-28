include(CTest)

if (WIN32)
    add_test(
        NAME ${PROJECT_NAME}tests
        COMMAND powershell.exe -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.ps1 $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/test_cases
    )
else()
    add_test(
        NAME ${PROJECT_NAME}tests
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/test_cases
    )
endif()
