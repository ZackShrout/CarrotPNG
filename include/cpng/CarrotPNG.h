//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Include internals only inside the library (not visible to users)
#if defined(CPNG_IMPLEMENTATION) || defined(CPNG_INTERNAL_BUILD)
#include "internal/types.h"
#include "internal/crc32.h"
#include "internal/bit_reader.h"
#include "internal/chunk_parser.h"
#include "internal/inflate.h"
#include "internal/defilter.h"
#endif

namespace cpng {
    // ──────────────────────────────────────────────────────────────────────────────
    // Public API
    // ──────────────────────────────────────────────────────────────────────────────

    // Just read header (width/height/etc) without decompressing
    [[nodiscard]] constexpr decode_error read_header_from_memory(
        std::span<const uint8_t> data,
        uint32_t& out_width,
        uint32_t& out_height,
        uint8_t& out_bit_depth,
        uint8_t& out_color_type
    ) noexcept
    {
        ihdr_info_t ihdr{};
        std::vector<std::span<const uint8_t>> dummy_idat; // not used

        auto err = cpng::parse_png_chunks(data, ihdr, dummy_idat);
        if (err != decode_error::ok) return err;

        if (!ihdr.valid) return decode_error::missing_ihdr;

        out_width     = ihdr.width;
        out_height    = ihdr.height;
        out_bit_depth = ihdr.bit_depth;
        out_color_type = ihdr.color_type;

        return decode_error::ok;
    }

    [[nodiscard]] constexpr decode_error load_from_memory(
        std::span<const uint8_t> data,
        image_view_t& out_view,
        std::vector<uint8_t>& out_pixel_storage
    ) noexcept
    {
        cpng::ihdr_info_t ihdr{};
        std::vector<std::span<const uint8_t>> idat_spans;

        auto err = cpng::parse_png_chunks(data, ihdr, idat_spans);
        if (err != decode_error::ok) return err;

        if (!ihdr.valid) return decode_error::missing_ihdr;

        // Quick MVP validation
        if (ihdr.bit_depth != 8 || (ihdr.color_type != 2 && ihdr.color_type != 6))
            return decode_error::unsupported_color_type;

        std::vector<uint8_t> idat_concat;
        err = cpng::concat_idat(idat_spans, idat_concat);
        if (err != decode_error::ok) return err;

        int bytes_per_pixel = (ihdr.color_type == 6) ? 4 : 3;  // MVP: only 2 and 6
        uint32_t row_bytes = 1 + ihdr.width * bytes_per_pixel;
        uint32_t expected_raw_size = ihdr.height * row_bytes;

        std::vector<uint8_t> decompressed;
        err = inflate_idat(idat_concat, decompressed, expected_raw_size, ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.color_type);
        if (err != decode_error::ok) return err;

        // After inflate + defilter, decompressed is now clean RGBA
        uint32_t stride = ihdr.width * ((ihdr.color_type == 6) ? 4 : 3);

        out_pixel_storage = std::move(decompressed);

        out_view = {
            .width        = ihdr.width,
            .height       = ihdr.height,
            .pixels       = out_pixel_storage,
            .stride_bytes = stride,
            .is_srgb      = true   // placeholder — later read sRGB chunk
        };

        return decode_error::ok;
    }

    [[nodiscard]] inline decode_error load_from_file(
        const char* path,
        image_view_t& out_view,
        std::vector<uint8_t>& out_pixel_storage // resized & filled with RGBA8
    ) noexcept
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return decode_error::file_not_found;  // add this enum entry if needed

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
            return decode_error::file_too_short;

        return load_from_memory(buffer, out_view, out_pixel_storage);
    }



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
            case decode_error::unsupported_filter:              return "unsupported filter";
            case decode_error::file_not_found:                  return "file not found";
            default:                                            return "unknown error";
        }
    }
} // namespace cpng
