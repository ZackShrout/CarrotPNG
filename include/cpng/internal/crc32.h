//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <array>
#include <span>

namespace cpng {
    constexpr std::array<uint32_t, 256> make_crc_table() noexcept
    {
        std::array<uint32_t, 256> table{ };

        for (uint32_t i{ 0 }; i < 256; ++i)
        {
            uint32_t c{ i };
            for (uint32_t j{ 0 }; j < 8; ++j)
            {
                c = c & 1u ? 0xEDB88320u ^ c >> 1u : c >> 1u;
            }
            table[i] = c;
        }

        return table;
    }

    inline constexpr std::array<uint32_t, 256> k_crc_table{ make_crc_table() };

    [[nodiscard]] constexpr uint32_t crc32(std::span<const uint8_t> data, uint32_t init_crc = 0xFFFFFFFFu) noexcept
    {
        uint32_t crc{ init_crc };

        for (const uint8_t byte: data)
        {
            crc = k_crc_table[(crc ^ byte) & 0xFFu] ^ crc >> 8u;
        }

        return crc ^ 0xFFFFFFFFu;
    }
} // namespace cpng
