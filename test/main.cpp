//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#define CPNG_IMPLEMENTATION
#include <cpng/CarrotPNG.h>

#include <fstream>
#include <print>
#include <vector>

#ifndef CARROTPNG_SOURCE_DIR
#define CARROTPNG_SOURCE_DIR "."
#endif

int main()
{
    const char* path = CARROTPNG_SOURCE_DIR "/reference_pngs/16x16orange.png";
    // const char* path = CARROTPNG_SOURCE_DIR "/reference_pngs/1x1orange.png";

    cpng::image_view_t view;
    std::vector<uint8_t> pixels;

    cpng::decode_error err{ cpng::load_from_file(path, view, pixels) };
    if (err != cpng::decode_error::ok)
    {
        std::println(stderr, "Load failed: {} ({})", cpng::to_string(err), static_cast<uint32_t>(err));
        return 1;
    }

    std::println("\nLoad success!");
    std::println("  Image: {}×{}  stride={}  pixels={} bytes", view.width, view.height, view.stride_bytes, view.pixels.size());

    if (view.pixels.size() >= 4)
    {
        std::println("  First pixel RGBA: {} {} {} {}",
                     static_cast<int>(view.pixels[0]),
                     static_cast<int>(view.pixels[1]),
                     static_cast<int>(view.pixels[2]),
                     static_cast<int>(view.pixels[3]));
    }

    return 0;
}
