cmake_minimum_required(VERSION 3.26)

project(
    bindfltapi-demo
    VERSION 1.0
    LANGUAGES CXX
)

set(PROJECT_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/source")

#
# Set up the executable
#
add_subdirectory("${PROJECT_SOURCE_PATH}")

#
# Produce build artifacts
#
install(
    TARGETS
        main_output_exe
    RUNTIME
        DESTINATION "/"
)

set(CPACK_PACKAGE_FILE_NAME "BindfltAPIDemo-${CMAKE_PROJECT_VERSION_MAJOR}_${CMAKE_PROJECT_VERSION_MINOR}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
