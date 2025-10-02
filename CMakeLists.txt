# CMakeLists.txt for YAML Parser Tests
# Minimum required CMake version
cmake_minimum_required(VERSION 3.14)

# Project name and version
project(YamlParserTests VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Fetch GoogleTest if not found
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Source files
set(TEST_SOURCES
    yaml_tests.cpp
)

# Include the YAML parser header directory (assume it's in the same dir or adjust path)
# If header is in a subdir, use add_subdirectory or target_include_directories

# Create the test executable
add_executable(yaml_tests ${TEST_SOURCES})

# Link GoogleTest libraries
target_link_libraries(yaml_tests
    gtest_main
    gtest
)

# Include directories (if needed, e.g., for the .hpp file)
target_include_directories(yaml_tests PRIVATE .)

# Add the test
add_test(NAME yaml_tests COMMAND yaml_tests)

# Optional: Set test properties
set_tests_properties(yaml_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Optional: Coverage (if using gcov/clang-cov)
# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     include(CTest)
#     target_compile_options(yaml_tests PRIVATE -fprofile-arcs -ftest-coverage)
#     target_link_options(yaml_tests PRIVATE -fprofile-arcs)
# endif()
