//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "bit_reader.h"
#include "fixed_tables.h"

#include <array>

namespace cpng {
    struct huffman_table_t
    {
        static constexpr int FAST_BITS{ 9 };
        static constexpr int FAST_MASK{ (1 << FAST_BITS) - 1 };

        std::array<uint16_t, 1 << FAST_BITS> fast{ }; // code | (len << 9)
        std::array<uint16_t, 16> first_code{ };
        std::array<int, 17> max_code{ };
        std::array<uint16_t, 16> first_symbol{ };
        std::array<uint8_t, 288> size{ }; // code length per symbol
        std::array<uint16_t, 288> value{ }; // symbol value
    };

    // Build the canonical Huffman table
    [[nodiscard]] constexpr huffman_table_t build_huffman_table(
        const int* lengths, const int num_symbols, const int max_fast_bits = 9) noexcept
    {
        huffman_table_t t{ };
        std::array<int, 16> bl_count{ };
        std::array<int, 16> next_code{ };

        for (int i{ 0 }; i < num_symbols; ++i)
            if (lengths[i] > 0) ++bl_count[lengths[i]];

        int code{ 0 };
        for (int bits{ 1 }; bits < 16; ++bits)
        {
            next_code[bits] = code;
            t.first_code[bits] = static_cast<uint16_t>(code);
            code += bl_count[bits];

            if (bl_count[bits])
                t.max_code[bits] = (code << (16 - bits)) - 1;

            code <<= 1;
        }

        t.max_code[16] = 0x10000; // sentinel

        int sym{ 0 };
        for (int i{ 0 }; i < num_symbols; ++i)
        {
            const int len{ lengths[i] };
            if (len == 0) continue;

            const int c{ next_code[len]++ };
            t.size[sym] = static_cast<uint8_t>(len);
            t.value[sym] = static_cast<uint16_t>(i);

            if (len <= max_fast_bits)
            {
                const int rev{ static_cast<int>(bit_reverse(c, len)) };
                const int incr{ 1 << len };

                for (int j{ rev }; j < (1 << max_fast_bits); j += incr)
                    t.fast[j] = static_cast<uint16_t>(len << 9 | sym);
            }
            ++sym;
        }

        return t;
    }

    // Fixed tables (built at compile-time)
    inline const auto fixed_lit_len_table{
        build_huffman_table(
            fixed_literal_lengths.data(), fixed_literal_lengths.size(), 9)
    };

    inline const auto fixed_dist_table{
        build_huffman_table(
            fixed_distance_lengths.data(), fixed_distance_lengths.size(), 5)
    };

    // ──────────────────────────────────────────────────────────────────────────────
    // Decode one symbol using the table (fast path + slow fallback)
    // ──────────────────────────────────────────────────────────────────────────────

    [[nodiscard]] inline int huffman_decode(
        bit_reader_t& reader, const huffman_table_t& table) noexcept
    {
        reader.fill_bits(); // Ensure we have as many bits as possible

        // Fast path: try with current bits
        if (reader.bits_in_buffer >= huffman_table_t::FAST_BITS)
        {
            const uint32_t peek{ reader.bit_buffer & huffman_table_t::FAST_MASK };
            const uint16_t entry{ table.fast[peek] };

            if (entry != 0)
            {
                const int len{ entry >> 9 };
                reader.bit_buffer >>= len;
                reader.bits_in_buffer -= len;

                return entry & 0x1FF;
            }
        }

        // Slow path: need to accumulate bits carefully
        int code{ 0 };
        int bits_used{ 0 };

        for (int s{ 1 }; s <= 15; ++s)
        {
            // If we don't have enough bits for this length, refill or fail
            if (reader.bits_in_buffer == 0)
            {
                reader.fill_bits();
                if (reader.bits_in_buffer == 0)
                    return -1; // EOF / underflow
            }

            // Take one bit
            const int bit{ static_cast<int>(reader.bit_buffer & 1) };
            reader.bit_buffer >>= 1;
            reader.bits_in_buffer--;
            code = code << 1 | bit;
            bits_used++;

            // Check if this prefix matches any code of length s
            if (code < table.max_code[s])
            {
                // Found a match
                const int sym{ code - table.first_code[s] + table.first_symbol[s] };

                return table.value[sym];
            }
        }

        return -1; // No code matched (corrupt stream)
    }
} // namespace cpng
