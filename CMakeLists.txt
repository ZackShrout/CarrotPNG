cmake_minimum_required(VERSION 3.20)
project(CarrotPNG LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Don't force compilers in CMakeLists; let the toolchain decide
# set(CMAKE_C_COMPILER clang)
# set(CMAKE_CXX_COMPILER clang++)

add_library(CarrotPNG STATIC
        src/CarrotPNG.cpp
)

add_library(CarrotPNG::CarrotPNG ALIAS CarrotPNG)

target_include_directories(CarrotPNG
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(CarrotPNG PUBLIC cxx_std_23)

# Only build tests if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    option(CARROTPNG_BUILD_TESTS "Build CarrotPNG validation tests" ON)
    if(CARROTPNG_BUILD_TESTS)
        enable_testing()
        add_subdirectory(test)
    endif()
endif()