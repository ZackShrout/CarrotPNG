//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#define CPNG_IMPLEMENTATION
#include <cpng/CarrotPNG.h>

#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>

#ifndef CARROTPNG_SOURCE_DIR
#define CARROTPNG_SOURCE_DIR "."
#endif

int main(int argc, char** argv)
{
    const char* path = CARROTPNG_SOURCE_DIR "/reference_pngs/16x16orange.png";
    // const char* path = CARROTPNG_SOURCE_DIR "/reference_pngs/1x1orange.png";

    // Command line file override
    if (argc >= 2)
        path = argv[1];

    // Read entire file into memory (simple for testing)
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << path << "\n";
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        std::cerr << "Failed to read file\n";
        return 1;
    }

    std::span<const uint8_t> data(buffer);

    cpng::ihdr_info_t ihdr{};
    std::vector<std::span<const uint8_t>> idat_spans;

    auto err = cpng::parse_png_chunks(data, ihdr, idat_spans);

    if (err != cpng::decode_error::ok)
    {
        std::cerr << "Chunk parse error: " << cpng::to_string(err)
                  << " (" << static_cast<uint32_t>(err) << ")\n";
        return 1;
    }

    std::cout << "Success! Parsed PNG:\n"
              << "  Width:  " << ihdr.width << "\n"
              << "  Height: " << ihdr.height << "\n"
              << "  Bit depth: " << static_cast<int>(ihdr.bit_depth) << "\n"
              << "  Color type: " << static_cast<int>(ihdr.color_type)
              << (ihdr.color_type == 6 ? " (truecolor + alpha)" :
                  ihdr.color_type == 2 ? " (truecolor)" :
                  ihdr.color_type == 3 ? " (palette)" : " ?") << "\n"
              << "  Compression: " << static_cast<int>(ihdr.compression_method) << "\n"
              << "  Filter: " << static_cast<int>(ihdr.filter_method) << "\n"
              << "  Interlace: " << static_cast<int>(ihdr.interlace_method)
              << (ihdr.interlace_method == 0 ? " (none)" : " (Adam7)") << "\n"
              << "  IDAT chunks found: " << idat_spans.size() << "\n";

    size_t total_idat = 0;
    for (auto sp : idat_spans) total_idat += sp.size();
    std::cout << "  Total IDAT bytes (compressed): " << total_idat << "\n";

    // ───────────────────────────────────────────────────────────────
    // Inflate test
    // ───────────────────────────────────────────────────────────────

    std::vector<uint8_t> idat_concat;
    auto concat_err = cpng::concat_idat(idat_spans, idat_concat);
    if (concat_err != cpng::decode_error::ok)
    {
        std::cerr << "Concat error: " << cpng::to_string(concat_err) << "\n";
        return 1;
    }

    // Expected raw filtered size: height × (1 filter byte + width × bytes_per_pixel)
    int bytes_per_pixel = (ihdr.color_type == 6) ? 4 : 3;   // MVP: only truecolor & truecolor+alpha
    uint32_t row_bytes = 1 + ihdr.width * bytes_per_pixel;
    uint32_t expected_raw_size = ihdr.height * row_bytes;

    std::vector<uint8_t> decompressed;
    auto inflate_err = cpng::inflate_idat(
        idat_concat,
        decompressed,
        expected_raw_size
    );

    if (inflate_err != cpng::decode_error::ok)
    {
        std::cerr << "Inflate error: " << cpng::to_string(inflate_err)
                  << " (" << static_cast<uint32_t>(inflate_err) << ")\n";
        return 1;
    }

    std::cout << "\nInflate success!\n"
              << "  Decompressed raw filtered size: " << decompressed.size() << " bytes\n"
              << "  (expected: " << expected_raw_size << " bytes)\n";

    if (decompressed.size() >= 65)  // first row
    {
        std::cout << "First row filter: " << (int)decompressed[0] << "\n";
        std::cout << "First pixel RGBA: "
                  << (int)decompressed[1] << " " << (int)decompressed[2] << " "
                  << (int)decompressed[3] << " " << (int)decompressed[4] << "\n";
    }

    // For small images, show first row or first few bytes
    if (!decompressed.empty())
    {
        std::cout << "  First 32 bytes of raw filtered data (hex):\n    ";
        size_t count = 0;
        for (uint8_t b : decompressed)
        {
            if (count >= 32) break;
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
            ++count;
            if (count % 16 == 0) std::cout << "\n    ";
        }
        std::cout << std::dec << "\n";

        // Show filter byte of first row
        std::cout << "  First row filter byte: " << static_cast<int>(decompressed[0]) << "\n";
    }

    return 0;
}