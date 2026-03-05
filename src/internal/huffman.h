//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "bit_reader.h"
#include "fixed_tables.h"

#include <array>
#include <print>

namespace cpng {
    struct huffman_table_t
    {
        static constexpr int FAST_BITS{ 15 };
        static constexpr int FAST_MASK{ (1 << FAST_BITS) - 1 };

        // Entry encoding: (len << 9) | symbol
        // len: 1..15, symbol: 0..287 (fits in 9 bits)
        std::array<uint16_t, 1 << FAST_BITS> fast{ };
    };

    // Build the canonical Huffman tables
    [[nodiscard]] inline huffman_table_t build_huffman_table(const int* lengths, const int num_symbols) noexcept
    {
        huffman_table_t t{ };

        // Count codes for each length
        std::array<int, 16> bl_count{ };
        for (int i{ 0 }; i < num_symbols; ++i)
        {
            const int len{ lengths[i] };

            if (len < 0 || len > 15) return t; // invalid -> table stays empty

            if (len) ++bl_count[len];
        }

        // Compute canonical first code for each length (RFC 1951)
        std::array<int, 16> next_code{ };
        int code{ 0 };
        for (int bits{ 1 }; bits <= 15; ++bits)
        {
            code = (code + bl_count[bits - 1]) << 1;
            next_code[bits] = code;
        }

        // Fill the 15-bit decode table using bit-reversed codes (DEFLATE is LSB-first)
        for (int sym{ 0 }; sym < num_symbols; ++sym)
        {
            const int len{ lengths[sym] };
            if (!len) continue;

            const int c{ next_code[len]++ };

            // Reverse the low 'len' bits
            const uint32_t rev{ bit_reverse(static_cast<uint32_t>(c), len) };

            // Replicate this len-bit code across the remaining FAST_BITS-len bits.
            // Because rev < (1<<len), stepping by (1<<len) enumerates all table entries
            // whose low 'len' bits match this code (LSB-first DEFLATE indexing).
            const int step{ 1 << len };
            for (int j{ static_cast<int>(rev) }; j < 1 << huffman_table_t::FAST_BITS; j += step)
                t.fast[j] = static_cast<uint16_t>((len << 9) | sym);
        }

        return t;
    }

    inline const auto fixed_lit_len_table{
        build_huffman_table(fixed_literal_lengths.data(), static_cast<int>(fixed_literal_lengths.size()))
    };

    inline const auto fixed_dist_table{
        build_huffman_table(fixed_distance_lengths.data(), static_cast<int>(fixed_distance_lengths.size()))
    };

    [[nodiscard]] inline int huffman_decode(bit_reader_t& reader, const huffman_table_t& table) noexcept
    {
        reader.fill_bits();

        if (reader.bits_in_buffer == 0) return -1;

        // We can still peek 15 bits even if we have fewer; mask is safe.
        const uint32_t peek{ reader.bit_buffer & huffman_table_t::FAST_MASK };
        const uint16_t entry{ table.fast[peek] };

        const int len{ entry >> 9 };

        if (len <= 0) return -1; // invalid code
        if (static_cast<uint32_t>(len) > reader.bits_in_buffer) return -1; // underflow

        reader.bit_buffer >>= len;
        reader.bits_in_buffer -= static_cast<uint32_t>(len);

        return entry & 0x1FF; // actual symbol
    }
} // namespace cpng
