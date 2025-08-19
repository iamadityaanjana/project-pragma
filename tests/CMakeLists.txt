cmake_minimum_required(VERSION 3.16)

# Find GTest
find_package(GTest REQUIRED)

# Include parent source directory
include_directories(../src)

# Test files
set(TEST_SOURCES
    test_hash.cpp
    test_serialize.cpp
    test_utils.cpp
)

# Create test executable
add_executable(pragma_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(pragma_tests 
    GTest::gtest
    GTest::gtest_main
    OpenSSL::SSL 
    OpenSSL::Crypto
)

# Add parent source files that tests depend on
target_sources(pragma_tests PRIVATE
    ../src/primitives/hash.cpp
    ../src/primitives/serialize.cpp
    ../src/primitives/utils.cpp
)

# Register tests with CTest
gtest_discover_tests(pragma_tests)
