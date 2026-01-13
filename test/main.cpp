//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#define CPNG_IMPLEMENTATION
#include <cpng/CarrotPNG.h>

#include <fstream>
#include <print>
#include <cstdint>
#include <vector>

#ifndef CARROTPNG_SOURCE_DIR
#define CARROTPNG_SOURCE_DIR "."
#endif

// ANSI foreground color: \x1b[38;2;R;G;Bm
// Background color:     \x1b[48;2;R;G;Bm
// Reset:                \x1b[0m

void print_pixel_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    // Use background color + a wide character (like █ or full block) for solid color
    std::print("\x1b[48;2;{};{};{}m  \x1b[0m", r, g, b);
}

void print_image_as_colored_blocks(
    const std::span<const uint8_t> rgba,
    uint32_t width,
    uint32_t height,
    const uint32_t max_width = 80 // terminal width limit
) noexcept
{
    if (rgba.size() < width * height * 4ull)
    {
        std::println(stderr, "Not enough data for {}×{} image (need {}, got {})",
                     width, height, width * height * 4ull, rgba.size());
        return;
    }

    // Each "pixel" takes ~2 chars wide (background color + space)
    const uint32_t display_width{ std::min(width, max_width / 2) };

    for (uint32_t y{ 0 }; y < height; ++y)
    {
        for (uint32_t x{ 0 }; x < display_width; ++x)
        {
            const size_t i{ (static_cast<size_t>(y) * width + x) * 4 };
            const uint8_t r{ rgba[i + 0] };
            const uint8_t g{ rgba[i + 1] };
            const uint8_t b{ rgba[i + 2] };
            // uint8_t a = rgba[i + 3];  // could use for dimming if wanted

            print_pixel_rgb(r, g, b);
        }
        std::println("");
    }
}

int main()
{
    // const char* path = CARROTPNG_SOURCE_DIR "/reference_pngs/16x16orange.png";
    // const char* path = CARROTPNG_SOURCE_DIR "/reference_pngs/1x1orange.png";
    const char* path = CARROTPNG_SOURCE_DIR "/reference_pngs/Vraden.png";


    cpng::image_view_t view;
    std::vector<uint8_t> pixels;

    cpng::decode_error err{ cpng::load_from_file(path, view, pixels) };
    if (err != cpng::decode_error::ok)
    {
        std::println(stderr, "Load failed: {} ({})", cpng::to_string(err), static_cast<uint32_t>(err));
        return 1;
    }

    std::println("\nLoad success!");
    std::println("  Image: {}x{}  stride={}  pixels={} bytes", view.width, view.height, view.stride_bytes,
                 view.pixels.size());

    if (view.pixels.size() >= 4)
    {
        std::println("  First pixel RGBA: {} {} {} {}",
                     static_cast<int>(view.pixels[0]),
                     static_cast<int>(view.pixels[1]),
                     static_cast<int>(view.pixels[2]),
                     static_cast<int>(view.pixels[3]));
    }

    std::println("\nMini colored preview (first {}×{} pixels):", view.width, view.height);
    print_image_as_colored_blocks(
        view.pixels,
        view.width,
        view.height,
        80 // adjust to your terminal width
    );

    return 0;
}
