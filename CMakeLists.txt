cmake_minimum_required(VERSION 3.16)
project(PragmaBlockchain VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(src)

# Source files
set(SOURCES
    src/primitives/hash.cpp
    src/primitives/serialize.cpp
    src/primitives/utils.cpp
    src/core/transaction.cpp
    src/core/merkle.cpp
)

# Headers
set(HEADERS
    src/primitives/hash.h
    src/primitives/serialize.h
    src/primitives/utils.h
    src/core/transaction.h
    src/core/merkle.h
)

# Main executable
add_executable(pragma-node src/main.cpp ${SOURCES})

# Link libraries
target_link_libraries(pragma-node 
    OpenSSL::SSL 
    OpenSSL::Crypto
)

# Tests (optional)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    find_package(GTest)
    if(GTest_FOUND)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(pragma-node PRIVATE -Wall -Wextra -O3)
endif()
