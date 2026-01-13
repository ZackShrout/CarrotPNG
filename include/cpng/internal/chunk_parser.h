//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "types.h"
#include "crc32.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cpng {
    [[nodiscard]] constexpr std::optional<uint32_t> read_be_uint32_t(const std::span<const uint8_t>& data,
                                                                     size_t& pos) noexcept
    {
        if (pos + 4 > data.size()) return std::nullopt;

        uint32_t val{
            (static_cast<uint32_t>(data[pos + 0]) << 24) |
            (static_cast<uint32_t>(data[pos + 1]) << 16) |
            (static_cast<uint32_t>(data[pos + 2]) << 8) |
            (static_cast<uint32_t>(data[pos + 3]) << 0)
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

        return file_data.size() >= 8 &&
               std::equal(signature.begin(), signature.end(), file_data.begin());
    }

    [[nodiscard]] constexpr decode_error parse_png_chunks(
        std::span<const uint8_t> file_data,
        ihdr_info_t& out_ihdr,
        std::vector<std::span<const uint8_t>>& out_idat_spans
    ) noexcept
    {
        if (!check_png_signature(file_data))
            return decode_error::invalid_signature;

        size_t pos{ 8 };
        bool seen_ihdr{ false };
        bool seen_iend{ false };

        while (pos < file_data.size())
        {
            std::optional<uint32_t> length_opt{ read_be_uint32_t(file_data, pos) };
            if (!length_opt) return decode_error::file_too_short;
            const uint32_t length{ *length_opt };

            if (pos + 4 + length + 4 > file_data.size())
                return decode_error::invalid_chunk_length;

            std::array<uint8_t, 4> type{ };
            std::copy_n(file_data.begin() + static_cast<uint32_t>(pos), 4, type.begin());
            pos += 4;

            std::span<const uint8_t> chunk_data{ file_data.begin() + static_cast<uint32_t>(pos), length };
            pos += length;

            std::optional<uint32_t> crc_opt{ read_be_uint32_t(file_data, pos) };
            if (!crc_opt) return decode_error::file_too_short;
            const uint32_t expected_crc{ *crc_opt };

            std::vector<uint8_t> crc_input;
            crc_input.reserve(4 + length);
            crc_input.insert(crc_input.end(), type.begin(), type.end());
            crc_input.insert(crc_input.end(), chunk_data.begin(), chunk_data.end());
            const uint32_t actual_crc{ crc32(crc_input) };

            if (actual_crc != expected_crc)
                return decode_error::crc_mismatch;

            const std::string_view type_sv{ reinterpret_cast<const char *>(type.data()), 4 };

            if (type_sv == "IHDR")
            {
                if (seen_ihdr) return decode_error::duplicate_ihdr;
                if (seen_iend) return decode_error::unexpected_chunk_order;
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

                seen_ihdr = true;
            }
            else if (type_sv == "IDAT")
            {
                if (!seen_ihdr) return decode_error::unexpected_chunk_order;
                if (seen_iend) return decode_error::unexpected_chunk_order;
                out_idat_spans.push_back(chunk_data);
            }
            else if (type_sv == "IEND")
            {
                if (length != 0) return decode_error::invalid_chunk_length;
                if (!seen_ihdr) return decode_error::missing_ihdr;
                seen_iend = true;
                break;
            }
            // ignore ancillary chunks for now
        }

        if (!seen_ihdr) return decode_error::missing_ihdr;
        if (!seen_iend) return decode_error::no_iend;
        if (out_idat_spans.empty()) return decode_error::no_idat_chunks;

        return decode_error::ok;
    }
} // namespace cpng
