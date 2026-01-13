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
        std::println(stderr, "Failed to open file: {}", path);
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
    {
        std::println(stderr, "Failed to read file: {}", path);
        return 1;
    }

    std::span<const uint8_t> data(buffer);

    cpng::ihdr_info_t ihdr{ };
    std::vector<std::span<const uint8_t>> idat_spans;

    cpng::decode_error err{ cpng::parse_png_chunks(data, ihdr, idat_spans) };

    if (err != cpng::decode_error::ok)
    {
        std::println(stderr, "Chunk parse error: {} ({})", cpng::to_string(err), static_cast<uint32_t>(err));
        return 1;
    }

    std::println("Success! Parsed PNG:");
    std::println("  Width: {}", ihdr.width);
    std::println("  Height: {}", ihdr.height);
    std::println("  Bit depth: {}", static_cast<int>(ihdr.bit_depth));
    std::println("  Color type: {} ({})", static_cast<int>(ihdr.color_type),
                 ihdr.color_type == 6
                     ? "truecolor + alpha"
                     : ihdr.color_type == 2
                           ? "truecolor"
                           : ihdr.color_type == 3
                                 ? "palette"
                                 : "?");
    std::println("  Compression: {}", static_cast<int>(ihdr.compression_method));
    std::println("  Filter: {}", static_cast<int>(ihdr.filter_method));
    std::println("  Interlace: {} ({})", static_cast<int>(ihdr.interlace_method),
                 ihdr.interlace_method == 0 ? "none" : "Adam7");
    std::println("  IDAT chunks found: {}", idat_spans.size());

    size_t total_idat = 0;
    for (auto sp: idat_spans) total_idat += sp.size();

    std::println("  Total IDAT bytes (compressed): {}", total_idat);

    // ───────────────────────────────────────────────────────────────
    // Inflate test
    // ───────────────────────────────────────────────────────────────

    std::vector<uint8_t> idat_concat;
    cpng::decode_error concat_err{ cpng::concat_idat(idat_spans, idat_concat) };

    if (concat_err != cpng::decode_error::ok)
    {
        std::println(stderr, "Concat error: {} ({})", cpng::to_string(concat_err), static_cast<uint32_t>(concat_err));
        return 1;
    }

    // Expected raw filtered size: height × (1 filter byte + width × bytes_per_pixel)
    int bytes_per_pixel{ ihdr.color_type == 6 ? 4 : 3 }; // MVP: only truecolor & truecolor+alpha
    uint32_t row_bytes{ 1 + ihdr.width * bytes_per_pixel };
    uint32_t expected_raw_size{ ihdr.height * row_bytes };

    std::vector<uint8_t> decompressed;
    cpng::decode_error inflate_err{ cpng::inflate_idat(idat_concat, decompressed, expected_raw_size) };

    if (inflate_err != cpng::decode_error::ok)
    {
        std::println(stderr, "Inflate error: {} ({})", cpng::to_string(inflate_err),
                     static_cast<uint32_t>(inflate_err));
        return 1;
    }

    std::println("\nInflate success!");
    std::println("  Decompressed raw filtered size: {} bytes", decompressed.size());
    std::println("  (expected: {} bytes)", expected_raw_size);

    if (decompressed.size() >= 65) // first row
    {
        std::println("First row filter: {}", static_cast<int>(decompressed[0]));
        std::println("First pixel RGBA: {} {} {} {}", static_cast<int>(decompressed[1]),
                     static_cast<int>(decompressed[2]), static_cast<int>(decompressed[3]),
                     static_cast<int>(decompressed[4]));
    }

    // For small images, show the first row or first few bytes
    if (!decompressed.empty())
    {
        std::println("  First 32 bytes of raw filtered data (hex):");
        size_t count = 0;
        for (uint8_t b: decompressed)
        {
            if (count >= 32) break;

            std::print("  {:02x} ", b);
            ++count;

            if (count % 16 == 0) std::println("");
        }
        std::println("");

        // Show filter byte of first row
        std::println("  First row filter byte: {}", static_cast<int>(decompressed[0]));
    }

    return 0;
}
