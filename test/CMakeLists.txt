# Path to the project source root (useful for test files)
set(CARROTPNG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(CarrotPNG_test main.cpp)
target_link_libraries(CarrotPNG_test PRIVATE CarrotPNG::CarrotPNG)
target_compile_definitions(CarrotPNG_test PRIVATE
        CARROTPNG_SOURCE_DIR="${CARROTPNG_SOURCE_DIR}"
)
add_test(NAME CarrotPNG_validation COMMAND CarrotPNG_test)