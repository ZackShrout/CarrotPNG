//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "cpng/CarrotPNG.h"
#include "crc32.h"

#include <array>
#include <span>
#include <vector>
#include <algorithm>
#include <optional>

namespace cpng {
    [[nodiscard]] constexpr std::optional<uint32_t> read_be_uint32_t(const std::span<const uint8_t>& data,
                                                                     size_t& pos) noexcept
    {
        if (pos + 4 > data.size()) return std::nullopt;

        const uint32_t val{
            static_cast<uint32_t>(data[pos + 0]) << 24 |
            static_cast<uint32_t>(data[pos + 1]) << 16 |
            static_cast<uint32_t>(data[pos + 2]) << 8 |
            static_cast<uint32_t>(data[pos + 3]) << 0
        };
        pos += 4;

        return val;
    }

    [[nodiscard]] constexpr std::optional<uint8_t> read_uint8_t(const std::span<const uint8_t>& data,
                                                                size_t& pos) noexcept
    {
        if (pos >= data.size()) return std::nullopt;

        return data[pos++];
    }

    [[nodiscard]] constexpr bool check_png_signature(std::span<const uint8_t> file_data) noexcept
    {
        constexpr std::array<uint8_t, 8> signature{ 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

        return file_data.size() >= 8 && std::equal(signature.begin(), signature.end(), file_data.begin());
    }

    [[nodiscard]] constexpr uint32_t peek_be_u32(const uint8_t* p) noexcept
    {
        return static_cast<uint32_t>(p[0]) << 24 |
               static_cast<uint32_t>(p[1]) << 16 |
               static_cast<uint32_t>(p[2]) << 8 |
               static_cast<uint32_t>(p[3]) << 0;
    }

    [[nodiscard]] constexpr decode_error parse_png_chunks(std::span<const uint8_t> file_data, ihdr_info_t& out_ihdr,
                                                          std::vector<std::span<const uint8_t>>& out_idat_spans) noexcept
    {
        out_ihdr = { };
        out_idat_spans.clear();
        out_idat_spans.reserve(8);

        if (!check_png_signature(file_data))
            return decode_error::invalid_signature;

        size_t pos{ 8 };
        bool seen_ihdr{ false };
        bool seen_iend{ false };

        while (pos < file_data.size())
        {
            // length
            const auto length_opt{ read_be_uint32_t(file_data, pos) };
            if (!length_opt) return decode_error::file_too_short;

            const uint32_t length{ *length_opt };

            // must have: type(4) + data(length) + crc(4)
            if (pos + 4u + static_cast<size_t>(length) + 4u > file_data.size())
                return decode_error::invalid_chunk_length;

            // type
            const uint8_t* type_ptr{ file_data.data() + pos };
            const uint32_t type_u32{ peek_be_u32(type_ptr) };
            pos += 4;

            // data
            const std::span<const uint8_t> chunk_data{ file_data.data() + pos, static_cast<size_t>(length) };
            pos += static_cast<size_t>(length);

            // crc in file
            const auto crc_opt{ read_be_uint32_t(file_data, pos) };
            if (!crc_opt) return decode_error::file_too_short;

            const uint32_t expected_crc{ *crc_opt };

            // crc(type + data) without allocations
            uint32_t crc{ 0xFFFFFFFFu };
            crc = crc32_update(crc, std::span<const uint8_t>{ type_ptr, 4 });
            crc = crc32_update(crc, chunk_data);

            if (crc32_finalize(crc) != expected_crc)
                return decode_error::crc_mismatch;

            switch (type_u32)
            {
                case 0x49484452u: // "IHDR"
                {
                    if (seen_ihdr) return decode_error::duplicate_ihdr;
                    // if (seen_iend) return decode_error::unexpected_chunk_order;
                    if (length != 13) return decode_error::invalid_chunk_length;

                    size_t ihdr_pos{ 0 };
                    auto w{ read_be_uint32_t(chunk_data, ihdr_pos) };
                    if (!w) return decode_error::invalid_chunk_length;
                    auto h{ read_be_uint32_t(chunk_data, ihdr_pos) };
                    if (!h) return decode_error::invalid_chunk_length;
                    auto bd{ read_uint8_t(chunk_data, ihdr_pos) };
                    if (!bd) return decode_error::invalid_chunk_length;
                    auto ct{ read_uint8_t(chunk_data, ihdr_pos) };
                    if (!ct) return decode_error::invalid_chunk_length;
                    auto cm{ read_uint8_t(chunk_data, ihdr_pos) };
                    if (!cm) return decode_error::invalid_chunk_length;
                    auto fm{ read_uint8_t(chunk_data, ihdr_pos) };
                    if (!fm) return decode_error::invalid_chunk_length;
                    auto im{ read_uint8_t(chunk_data, ihdr_pos) };
                    if (!im) return decode_error::invalid_chunk_length;

                    out_ihdr.width = *w;
                    out_ihdr.height = *h;
                    out_ihdr.bit_depth = *bd;
                    out_ihdr.color_type = *ct;
                    out_ihdr.compression_method = *cm;
                    out_ihdr.filter_method = *fm;
                    out_ihdr.interlace_method = *im;
                    out_ihdr.valid = true;

                    if (out_ihdr.width == 0 || out_ihdr.height == 0)
                        return decode_error::invalid_chunk_length;

                    seen_ihdr = true;
                    break;
                }

                case 0x49444154u: // "IDAT"
                {
                    if (!seen_ihdr) return decode_error::unexpected_chunk_order;
                    if (seen_iend) return decode_error::unexpected_chunk_order;
                    out_idat_spans.push_back(chunk_data);
                    break;
                }

                case 0x49454E44u: // "IEND"
                {
                    if (length != 0) return decode_error::invalid_chunk_length;
                    if (!seen_ihdr) return decode_error::missing_ihdr;
                    if (out_idat_spans.empty()) return decode_error::no_idat_chunks;
                    seen_iend = true;

                    break;
                }

                case 0x73524742u: // "sRGB"
                {
                    if (seen_ihdr && out_idat_spans.empty() && length == 1)
                        out_ihdr.has_srgb = true;

                    break;
                }

                case 0x67414D41u: // "gAMA"
                {
                    if (seen_ihdr && out_idat_spans.empty() && length == 4)
                    {
                        size_t gp{ 0 };
                        if (auto g = read_be_uint32_t(chunk_data, gp))
                        {
                            out_ihdr.has_gamma = true;
                            out_ihdr.gamma = static_cast<float>(*g) / 100000.0f;
                        }
                    }

                    break;
                }

                // "iCCP"
                case 0x69434350u:
                {
                    if (!seen_ihdr || !out_idat_spans.empty()) break;

                    // Must contain at least: "x\0" + method(1) + 1 byte data = 4 bytes min
                    if (length < 4) break;

                    // Find null terminator in first 80 bytes (per spec)
                    const size_t max_name{ std::min<size_t>(80, chunk_data.size()) };
                    size_t zero{ 0 };
                    for (; zero < max_name; ++zero)
                        if (chunk_data[zero] == 0) break;

                    if (zero == max_name) break; // no terminator found quickly

                    const size_t method_pos{ zero + 1 };
                    if (method_pos >= chunk_data.size()) break;

                    if (chunk_data[method_pos] != 0) break;

                    out_ihdr.has_icc_profile = true;
                    break;
                }

                default:
                    break;
            }

            if (seen_iend)
                break;
        }

        if (!seen_ihdr) return decode_error::missing_ihdr;
        if (!seen_iend) return decode_error::no_iend;
        if (out_idat_spans.empty()) return decode_error::no_idat_chunks;

        return decode_error::ok;
    }
} // namespace cpng
