//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cpng {
    // ──────────────────────────────────────────────────────────────────────────────
    // Public types & enums
    // ──────────────────────────────────────────────────────────────────────────────

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
    };

    // ──────────────────────────────────────────────────────────────────────────────
    // Public API
    // ──────────────────────────────────────────────────────────────────────────────

    [[nodiscard]] decode_error load_from_file(
        const char* path,
        image_view_t& out_view,
        std::vector<uint8_t>& out_pixel_storage // resized & filled with RGBA8
    ) noexcept;

    [[nodiscard]] decode_error load_from_memory(
        std::span<const uint8_t> data,
        image_view_t& out_view,
        std::vector<uint8_t>& out_pixel_storage
    ) noexcept;

    // Optional: just read header (width/height/etc) without decompressing
    [[nodiscard]] decode_error read_header_from_memory(
        std::span<const uint8_t> data,
        uint32_t& out_width,
        uint32_t& out_height,
        uint8_t& out_bit_depth,
        uint8_t& out_color_type
    ) noexcept;

    // Human-readable error string
    [[nodiscard]] constexpr std::string_view to_string(const decode_error err) noexcept
    {
        switch (err)
        {
            case decode_error::ok:                              return "ok";
            case decode_error::invalid_signature:               return "invalid PNG signature";
            case decode_error::file_too_short:                  return "file too short / truncated";
            case decode_error::invalid_chunk_length:            return "invalid chunk length";
            case decode_error::crc_mismatch:                    return "CRC mismatch";
            case decode_error::duplicate_ihdr:                  return "duplicate IHDR chunk";
            case decode_error::missing_ihdr:                    return "missing IHDR chunk";
            case decode_error::unexpected_chunk_order:          return "unexpected chunk order";
            case decode_error::unsupported_color_type:          return "unsupported color type";
            case decode_error::unsupported_bit_depth:           return "unsupported bit depth";
            case decode_error::no_idat_chunks:                  return "no IDAT chunks found";
            case decode_error::invalid_idat_stream:             return "invalid IDAT / zlib stream";
            case decode_error::no_iend:                         return "missing IEND chunk";
            case decode_error::unsupported_compression_filter:  return "unsupported compression filter";
            default:                                            return "unknown error";
        }
    }
} // namespace cpng

// Include internals only inside the library (not visible to users)
#if defined(CPNG_IMPLEMENTATION) || defined(CPNG_INTERNAL_BUILD)
#include "internal/types.h"
#include "internal/crc32.h"
#include "internal/bit_reader.h"
#include "internal/chunk_parser.h"
#include "internal/inflate.h"
#endif
