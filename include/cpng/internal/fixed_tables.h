//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <array>

namespace cpng {
    // Fixed Huffman code lengths from RFC 1951 §3.2.6

    inline constexpr std::array<int, 288> fixed_literal_lengths{
        []() {
            std::array<int, 288> lens{ };
            for (int i = 0; i <= 143; ++i) lens[i] = 8;
            for (int i = 144; i <= 255; ++i) lens[i] = 9;
            for (int i = 256; i <= 279; ++i) lens[i] = 7;
            for (int i = 280; i <= 287; ++i) lens[i] = 8;
            return lens;
        }()
    };

    inline constexpr std::array<int, 32> fixed_distance_lengths{
        []() {
            std::array<int, 32> lens{ };
            for (int& len: lens) len = 5;
            return lens;
        }()
    };

    // Length codes 257..285 base values + extra bits
    inline constexpr std::array<int, 29> length_base{
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
        15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
        67, 83, 99, 115, 131, 163, 195, 227, 258
    };

    inline constexpr std::array<int, 29> length_extra{
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };

    // Distance codes 0..29 base values + extra bits
    inline constexpr std::array<int, 30> dist_base{
        1, 2, 3, 4, 5, 7, 9, 13,
        17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073,
        4097, 6145, 8193, 12289, 16385, 24577
    };

    inline constexpr std::array<int, 30> dist_extra{
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };
} // namespace cpng
