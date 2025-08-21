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
    src/core/block.cpp
    src/core/difficulty.cpp
    src/core/chainstate.cpp
    src/core/utxo.cpp
    src/core/validator.cpp
    src/core/mempool.cpp
    src/core/retargeting.cpp
)

# Headers
set(HEADERS
    src/primitives/hash.h
    src/primitives/serialize.h
    src/primitives/utils.h
    src/core/transaction.h
    src/core/merkle.h
    src/core/block.h
    src/core/difficulty.h
    src/core/chainstate.h
    src/core/utxo.h
    src/core/validator.h
    src/core/mempool.h
    src/core/retargeting.h
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
        
        add_executable(pragma_tests
            tests/test_transaction.cpp
            tests/test_merkle.cpp
            tests/test_block.cpp
            tests/test_difficulty.cpp
            tests/test_chainstate.cpp
            tests/test_utxo.cpp
            ${SOURCES}
        )
        
        target_link_libraries(pragma_tests 
            GTest::gtest 
            GTest::gtest_main 
            OpenSSL::SSL 
            OpenSSL::Crypto
        )
        
        # Discover tests
        include(GoogleTest)
        gtest_discover_tests(pragma_tests)
    else()
        message(STATUS "GTest not found, tests will not be built")
    endif()
endif()

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(pragma-node PRIVATE -Wall -Wextra -O3)
endif()
