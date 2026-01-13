//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace cpng {
    struct image_view_t
    {
        uint32_t width{ };
        uint32_t height{ };
        std::span<const uint8_t> pixels{ }; // contiguous RGBA8, row-major
        uint32_t stride_bytes{ };
        bool is_srgb{ true }; // default assumption
        // future: gamma, phys ppm, etc.
    };

    enum class decode_error : uint32_t
    {
        ok,
        invalid_signature,
        file_too_short,
        invalid_chunk_length,
        crc_mismatch,
        duplicate_ihdr,
        missing_ihdr,
        unexpected_chunk_order,
        unsupported_color_type,
        unsupported_bit_depth,
        no_idat_chunks,
        invalid_idat_stream,
        no_iend,
        unsupported_compression_filter,
        unsupported_filter,
        file_not_found,
    };

    struct ihdr_info_t
    {
        uint32_t    width{ };
        uint32_t    height{ };
        uint8_t     bit_depth{ };
        uint8_t     color_type{ };
        uint8_t     compression_method{ };
        uint8_t     filter_method{ };
        uint8_t     interlace_method{ };
        bool        valid{ false };
    };
} // namespace cpng
