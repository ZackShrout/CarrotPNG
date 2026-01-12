cmake_minimum_required(VERSION 4.1)
project(CarrotPNG)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

SET(CARROT_PNG_SOURCES
        main.cpp
        CarrotPNG.h
)
add_executable(CarrotPNG ${CARROT_PNG_SOURCES})
