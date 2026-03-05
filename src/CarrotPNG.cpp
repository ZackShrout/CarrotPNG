//
// Created by Zack Shrout on 3/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "cpng/CarrotPNG.h"

#include "internal/crc32.h"
#include "internal/bit_reader.h"
#include "internal/chunk_parser.h"
#include "internal/inflate.h"
#include "internal/defilter.h"

#include <fstream>
#include <array>

namespace cpng {
    [[nodiscard]] decode_error read_ihdr_from_memory(const std::span<const uint8_t> data,
                                                     ihdr_info_t& out_ihdr) noexcept
    {
        out_ihdr = { }; // clear

        if (!check_png_signature(data))
            return decode_error::invalid_signature;

        size_t pos{ 8 };

        // length(4) + type(4) + IHDR(13) + crc(4)
        if (pos + 4 + 4 + 13 + 4 > data.size())
            return decode_error::file_too_short;

        const auto len_opt{ read_be_uint32_t(data, pos) };

        if (!len_opt) return decode_error::file_too_short;

        const uint32_t length{ *len_opt };

        if (pos + 4 > data.size()) return decode_error::file_too_short;

        const char t0{ static_cast<char>(data[pos + 0]) };
        const char t1{ static_cast<char>(data[pos + 1]) };
        const char t2{ static_cast<char>(data[pos + 2]) };
        const char t3{ static_cast<char>(data[pos + 3]) };
        pos += 4;

        if (!(t0 == 'I' && t1 == 'H' && t2 == 'D' && t3 == 'R'))
            return decode_error::missing_ihdr;

        if (length != 13)
            return decode_error::invalid_chunk_length;

        if (pos + 13 > data.size()) return decode_error::file_too_short;

        std::span<const uint8_t> ihdr_data{ data.begin() + static_cast<uint8_t>(pos), 13 };
        pos += 13;

        // Verify IHDR CRC (optional, but recommended)
        const auto crc_opt{ read_be_uint32_t(data, pos) };

        if (!crc_opt) return decode_error::file_too_short;

        const uint32_t expected_crc{ *crc_opt };

        // crc("IHDR" + ihdr_data) without temporary buffer
        constexpr std::array<uint8_t, 4> k_IHDR{ 'I', 'H', 'D', 'R' };

        uint32_t crc{ 0xFFFFFFFFu };
        crc = crc32_update(crc, std::span<const uint8_t>{ k_IHDR.data(), k_IHDR.size() });
        crc = crc32_update(crc, ihdr_data);

        if (crc32_finalize(crc) != expected_crc)
            return decode_error::crc_mismatch;

        // Parse IHDR fields
        size_t ihdr_pos{ 0 };
        const auto w{ read_be_uint32_t(ihdr_data, ihdr_pos) };
        const auto h{ read_be_uint32_t(ihdr_data, ihdr_pos) };
        const auto bd{ read_uint8_t(ihdr_data, ihdr_pos) };
        const auto ct{ read_uint8_t(ihdr_data, ihdr_pos) };
        const auto cm{ read_uint8_t(ihdr_data, ihdr_pos) };
        const auto fm{ read_uint8_t(ihdr_data, ihdr_pos) };
        const auto im{ read_uint8_t(ihdr_data, ihdr_pos) };

        if (!w || !h || !bd || !ct || !cm || !fm || !im)
            return decode_error::invalid_chunk_length;

        out_ihdr = {
            .width = *w,
            .height = *h,
            .bit_depth = *bd,
            .color_type = *ct,
            .compression_method = *cm,
            .filter_method = *fm,
            .interlace_method = *im,
            .valid = true
        };

        return decode_error::ok;
    }

    [[nodiscard]] decode_error load_from_memory(const std::span<const uint8_t> data, image_view_t& out_view,
                                                std::vector<uint8_t>& out_pixel_storage) noexcept
    {
        ihdr_info_t ihdr{ };
        std::vector<std::span<const uint8_t>> idat_spans;

        decode_error err{ parse_png_chunks(data, ihdr, idat_spans) };
        if (err != decode_error::ok) return err;

        if (!ihdr.valid) return decode_error::missing_ihdr;

        if (ihdr.compression_method != 0 || ihdr.filter_method != 0)
            return decode_error::unsupported_compression_filter;

        if (ihdr.interlace_method != 0)
            return decode_error::unsupported_interlace;

        if (ihdr.width == 0 || ihdr.height == 0)
            return decode_error::invalid_chunk_length;

        // Quick MVP validation
        if (ihdr.bit_depth != 8) return decode_error::unsupported_bit_depth;
        if (ihdr.color_type != 2 && ihdr.color_type != 6) return decode_error::unsupported_color_type;

        std::vector<uint8_t> idat_concat;
        err = concat_idat(idat_spans, idat_concat);

        if (err != decode_error::ok) return err;

        const int bytes_per_pixel{ ihdr.color_type == 6 ? 4 : 3 }; // MVP: only 2 and 6
        const size_t row_bytes{ 1 + ihdr.width * bytes_per_pixel };
        const size_t expected_raw_size{ ihdr.height * row_bytes };

        std::vector<uint8_t> decompressed;
        err = inflate_idat(idat_concat, decompressed, expected_raw_size, ihdr.width, ihdr.height, ihdr.bit_depth,
                           ihdr.color_type);

        if (err != decode_error::ok) return err;

        if (ihdr.color_type == 2)
        {
            std::vector<uint8_t> rgba;
            rgba.resize(static_cast<size_t>(ihdr.width) * ihdr.height * 4);

            const uint8_t* src{ decompressed.data() }; // RGB
            uint8_t* dst{ rgba.data() };

            const size_t px_count{ static_cast<size_t>(ihdr.width) * ihdr.height };
            for (size_t i{ 0 }; i < px_count; ++i)
            {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
                src += 3;
                dst += 4;
            }

            decompressed = std::move(rgba);
        }

        // After inflate + defilter, decompressed is now clean RGBA
        const size_t stride_sz{ static_cast<size_t>(ihdr.width) * 4 };
        const uint32_t stride{ static_cast<uint32_t>(stride_sz) };

        out_pixel_storage = std::move(decompressed);

        bool is_srgb{ true };

        if (ihdr.has_srgb)
        {
            is_srgb = true;
        }
        else if (ihdr.has_gamma)
        {
            // PNG gamma chunk is "image gamma"; sRGB-ish gamma is ~0.45455.
            // If gamma is ~1.0, the stored values are already linear.
            if (ihdr.gamma > 0.95f && ihdr.gamma < 1.05f)
                is_srgb = false;
            // else: leave as true for now (no color management)
        }

        out_view = {
            .width = ihdr.width,
            .height = ihdr.height,
            .pixels = out_pixel_storage,
            .stride_bytes = stride,
            .is_srgb = is_srgb
        };

        return decode_error::ok;
    }

    [[nodiscard]] decode_error load_from_file(const char* path, image_view_t& out_view,
                                              std::vector<uint8_t>& out_pixel_storage) noexcept
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file.is_open()) return decode_error::file_not_found;

        const std::streamsize size{ file.tellg() };
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
            return decode_error::file_too_short;

        return load_from_memory(buffer, out_view, out_pixel_storage);
    }

    [[nodiscard]] decode_error read_ihdr_from_file(const char* path, ihdr_info_t& out_ihdr) noexcept
    {
        out_ihdr = { };

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return decode_error::file_not_found;

        // Signature (8) + length(4) + type(4) + IHDR(13) + CRC(4) = 33 bytes
        constexpr size_t k_min_bytes{ 8 + 4 + 4 + 13 + 4 };

        std::array<uint8_t, k_min_bytes> buf{ };
        if (!file.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(buf.size())))
            return decode_error::file_too_short;

        return read_ihdr_from_memory(std::span<const uint8_t>{ buf.data(), buf.size() }, out_ihdr);
    }

    [[nodiscard]] decode_error load_from_memory(const std::span<const uint8_t> data, image_view_t& out_view,
                                                const std::span<uint8_t> out_rgba8) noexcept
    {
        // First parse IHDR + IDAT spans so we know the required size.
        ihdr_info_t ihdr{ };
        std::vector<std::span<const uint8_t>> idat_spans;

        decode_error err{ parse_png_chunks(data, ihdr, idat_spans) };

        if (err != decode_error::ok) return err;

        if (!ihdr.valid) return decode_error::missing_ihdr;

        if (ihdr.compression_method != 0 || ihdr.filter_method != 0)
            return decode_error::unsupported_compression_filter;

        if (ihdr.interlace_method != 0)
            return decode_error::unsupported_interlace;

        if (ihdr.width == 0 || ihdr.height == 0)
            return decode_error::invalid_chunk_length;

        // MVP validation
        if (ihdr.bit_depth != 8) return decode_error::unsupported_bit_depth;
        if (ihdr.color_type != 2 && ihdr.color_type != 6) return decode_error::unsupported_color_type;

        const size_t needed = rgba8_size_bytes(ihdr);
        if (out_rgba8.size() < needed)
            return decode_error::output_buffer_too_small;

        // Decode using your existing vector-based path
        std::vector<uint8_t> tmp_pixels;
        image_view_t tmp_view{ };

        err = load_from_memory(data, tmp_view, tmp_pixels);
        if (err != decode_error::ok) return err;

        // tmp_pixels should be RGBA8 with size == needed
        if (tmp_pixels.size() < needed)
            return decode_error::invalid_idat_stream;

        std::copy_n(tmp_pixels.data(), needed, out_rgba8.data());

        out_view = {
            .width = ihdr.width,
            .height = ihdr.height,
            .pixels = std::span<const uint8_t>{ out_rgba8.data(), needed },
            .stride_bytes = ihdr.width * 4u,
            .is_srgb = true // placeholder; later could use ihdr.has_srgb / gamma flags
        };

        return decode_error::ok;
    }

    [[nodiscard]] std::string_view to_string(const decode_error err) noexcept
    {
        switch (err)
        {
            case decode_error::ok:                              return "ok";
            case decode_error::invalid_signature:               return "invalid PNG signature";
            case decode_error::file_too_short:                  return "file too short / truncated";
            case decode_error::output_buffer_too_small:         return "output buffer too small";
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
            case decode_error::unsupported_interlace:           return"unsupported interlace method";
            case decode_error::unsupported_filter:              return "unsupported filter";
            case decode_error::file_not_found:                  return "file not found";
            default:                                            return "unknown error";
        }
    }
} // namespace cpng
