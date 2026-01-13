cmake_minimum_required(VERSION 4.1)
project(CarrotPNG LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Header-only library interface
add_library(CarrotPNG INTERFACE)
target_include_directories(CarrotPNG INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(CarrotPNG INTERFACE cxx_std_23)

add_library(CarrotPNG::CarrotPNG ALIAS CarrotPNG)

# Only build tests if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    option(CARROTPNG_BUILD_TESTS "Build CarrotPNG validation tests" ON)
    if(CARROTPNG_BUILD_TESTS)
        enable_testing()
        add_subdirectory(test)
    endif()
endif()