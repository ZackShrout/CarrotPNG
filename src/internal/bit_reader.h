//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <optional>
#include <span>

namespace cpng {
    struct bit_reader_t
    {
        std::span<const uint8_t> data{ };
        size_t byte_pos{ 0 };
        uint32_t bit_buffer{ 0 };
        uint32_t bits_in_buffer{ 0 };

        [[nodiscard]] bool has_more() const noexcept
        {
            return byte_pos < data.size() || bits_in_buffer > 0;
        }

        void fill_bits() noexcept
        {
            while (bits_in_buffer < 24 && byte_pos < data.size())
            {
                bit_buffer |= static_cast<uint32_t>(data[byte_pos++]) << bits_in_buffer;
                bits_in_buffer += 8;
            }
        }

        [[nodiscard]] std::optional<uint32_t> get_bits(uint32_t n) noexcept
        {
            while (bits_in_buffer < n)
            {
                if (byte_pos >= data.size()) return std::nullopt;

                bit_buffer |= static_cast<uint32_t>(data[byte_pos++]) << bits_in_buffer;
                bits_in_buffer += 8;
            }

            uint32_t result{ bit_buffer & ((1u << n) - 1u) };
            bit_buffer >>= n;
            bits_in_buffer -= n;

            return result;
        }

        void align_to_byte() noexcept
        {
            bit_buffer = 0;
            bits_in_buffer = 0;
        }
    };

    [[nodiscard]] constexpr uint32_t bit_reverse(uint32_t v, int bits) noexcept
    {
        v = ((v & 0xAAAAu) >> 1) | ((v & 0x5555u) << 1);
        v = ((v & 0xCCCCu) >> 2) | ((v & 0x3333u) << 2);
        v = ((v & 0xF0F0u) >> 4) | ((v & 0x0F0Fu) << 4);
        v = ((v & 0xFF00u) >> 8) | ((v & 0x00FFu) << 8);
        return v >> (16 - bits);
    }
} // namespace cpng
