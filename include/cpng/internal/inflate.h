//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "huffman.h"

#include <vector>
#include <print>

namespace cpng {
    [[nodiscard]] constexpr decode_error concat_idat(
        std::span<const std::span<const uint8_t>> idat_spans,
        std::vector<uint8_t>& out_concat
    ) noexcept
    {
        size_t total{ 0 };
        for (auto sp: idat_spans) total += sp.size();

        out_concat.clear();
        out_concat.reserve(total);

        for (auto sp: idat_spans)
            out_concat.insert(out_concat.end(), sp.begin(), sp.end());

        return decode_error::ok;
    }

    [[nodiscard]] constexpr decode_error inflate_idat(
        std::span<const uint8_t> zlib_data, // full concatenated IDAT
        std::vector<uint8_t>& out_decompressed,
        const uint32_t expected_size // height * (width * bpp/8 + 1)
    ) noexcept
    {
        if (zlib_data.size() < 6) return decode_error::invalid_idat_stream;

        const uint8_t cmf{ zlib_data[0] };
        const uint8_t flg{ zlib_data[1] };

        if ((cmf & 0x0F) != 8) return decode_error::invalid_idat_stream;

        uint16_t check = (static_cast<uint16_t>(cmf) << 8) | flg;

        if (check % 31 != 0) return decode_error::invalid_idat_stream;
        if (flg & 0x20) return decode_error::invalid_idat_stream; // no dict

        std::span<const uint8_t> deflate_data{ zlib_data.begin() + 2, zlib_data.size() - 6 };

        bit_reader_t reader{ };
        reader.data = deflate_data;

        out_decompressed.clear();
        out_decompressed.reserve(expected_size);

        // Simple LZ77 window (for tiny images we can just use vector indexing)
        // For larger images we'll add a proper ring buffer later

        while (true)
        {
            std::optional<uint32_t> bfinal_opt{ reader.get_bits(1) };

            if (!bfinal_opt)
            {
                std::println(stderr, "Block header: not enough bits for BFINAL");
                return decode_error::invalid_idat_stream;
            }

            const bool is_final{ *bfinal_opt != 0 };

            std::optional<uint32_t> btype_opt{ reader.get_bits(2) };

            if (!btype_opt)
            {
                std::println(stderr, "Block header: not enough bits for BTYPE");
                return decode_error::invalid_idat_stream;
            }

            const uint32_t btype{ *btype_opt };

            if (btype == 0) // stored (uncompressed)
            {
                reader.align_to_byte();

                std::optional<uint32_t> len_opt{ reader.get_bits(16) };
                if (!len_opt) return decode_error::invalid_idat_stream;

                std::optional<uint32_t> nlen_opt{ reader.get_bits(16) };
                if (!nlen_opt) return decode_error::invalid_idat_stream;

                const uint16_t len{ static_cast<uint16_t>(*len_opt) };
                const uint16_t nlen{ static_cast<uint16_t>(*nlen_opt) };

                if (len != static_cast<uint16_t>(~nlen)) return decode_error::invalid_idat_stream;

                if (reader.byte_pos + len > deflate_data.size())
                    return decode_error::invalid_idat_stream;

                out_decompressed.insert(out_decompressed.end(),
                                        deflate_data.begin() + static_cast<uint8_t>(reader.byte_pos),
                                        deflate_data.begin() + static_cast<uint8_t>(reader.byte_pos) + len);
                reader.byte_pos += len;
            }
            else if (btype == 1) // fixed Huffman
            {
                while (true)
                {
                    const int sym{ huffman_decode(reader, fixed_lit_len_table) };

                    if (sym < 0)
                    {
                        if (out_decompressed.size() < expected_size)
                        {
                            std::println(stderr, "Premature EOF: got {} < {}", out_decompressed.size(), expected_size);
                            return decode_error::invalid_idat_stream;
                        }

                        break;
                    }

                    if (sym == 256) break;

                    if (sym < 256)
                    {
                        out_decompressed.push_back(static_cast<uint8_t>(sym));

                        if (out_decompressed.size() >= expected_size) break;
                    }
                    else // length code 257..285
                    {
                        const int idx{ sym - 257 };
                        if (idx >= 29) return decode_error::invalid_idat_stream;

                        int len{ length_base[idx] };
                        if (length_extra[idx])
                        {
                            std::optional<uint32_t> ex_opt{ reader.get_bits(length_extra[idx]) };
                            if (!ex_opt) return decode_error::invalid_idat_stream;

                            len += static_cast<int>(*ex_opt);
                        }

                        const int dist_sym{ huffman_decode(reader, fixed_dist_table) };
                        if (dist_sym < 0 || dist_sym >= 30) return decode_error::invalid_idat_stream;

                        int dist{ dist_base[dist_sym] };
                        if (dist_extra[dist_sym])
                        {
                            std::optional<uint32_t> ex_opt{ reader.get_bits(dist_extra[dist_sym]) };
                            if (!ex_opt) return decode_error::invalid_idat_stream;

                            dist += static_cast<int>(*ex_opt);
                        }

                        if (dist > static_cast<int>(out_decompressed.size()))
                            return decode_error::invalid_idat_stream;

                        if (static_cast<int64_t>(out_decompressed.size()) < dist)
                        {
                            std::println(stderr, "Invalid distance: size={}, dist={}", out_decompressed.size(), dist);
                            return decode_error::invalid_idat_stream;
                        }

                        size_t dst_pos{ out_decompressed.size() };

                        for (int i{ 0 }; i < len; ++i)
                        {
                            const size_t src_pos{ dst_pos - dist };
                            out_decompressed.push_back(out_decompressed[src_pos]);
                            ++dst_pos;
                        }

                        if (out_decompressed.size() >= expected_size) break;
                    }
                }
            }
            else if (btype == 2) // dynamic Huffman
            {
                return decode_error::unsupported_compression_filter;
            }
            else
            {
                return decode_error::invalid_idat_stream;
            }

            if (is_final) break;
        }

        // ───────────────────────────────────────────────────────────────
        // Stream completion & validation
        // ───────────────────────────────────────────────────────────────

        // Calculate consumption
        size_t consumed_bytes{ reader.byte_pos };
        if (reader.bits_in_buffer > 0)
            consumed_bytes += 1;

        if (consumed_bytes > deflate_data.size() + 1)
        {
            std::println(stderr, "Severe over-consume: byte_pos={}, bits_left={}", reader.byte_pos,
                         reader.bits_in_buffer);
            return decode_error::invalid_idat_stream;
        }

        size_t remaining{ deflate_data.size() - consumed_bytes };

        // Adler-32 verification (source of truth)
        if (zlib_data.size() < 4) return decode_error::invalid_idat_stream;

        const uint32_t adler_expected{
            (static_cast<uint32_t>(zlib_data[zlib_data.size() - 4]) << 24) |
            (static_cast<uint32_t>(zlib_data[zlib_data.size() - 3]) << 16) |
            (static_cast<uint32_t>(zlib_data[zlib_data.size() - 2]) << 8) |
            (static_cast<uint32_t>(zlib_data[zlib_data.size() - 1]))
        };

        uint32_t s1{ 1 };
        uint32_t s2{ 0 };
        for (const uint8_t b: out_decompressed)
        {
            s1 = (s1 + b) % 65521;
            s2 = (s2 + s1) % 65521;
        }

        const uint32_t adler_computed{ s2 << 16 | s1 };

        if (adler_computed != adler_expected)
        {
            std::println(stderr, "Alder-32 mismatch: computed={}, expected={}", adler_computed, adler_expected);
            return decode_error::invalid_idat_stream;
        }

        // Keep this only during development
        // std::println("Adler-32 verified (computed={})", adler_computed);

        // Final size handling
        if (out_decompressed.size() < expected_size)
        {
            std::println(stderr, "Underrun: got {}, expected {}", out_decompressed.size(), expected_size);
            return decode_error::invalid_idat_stream;
        }

        if (out_decompressed.size() > expected_size)
        {
            // std::println("Trimming {} extra byte(s)", out_decompressed.size() - expected_size);
            out_decompressed.resize(expected_size);
        }

        // Keep this only during development
        // if (remaining > 0)
        // {
        //     std::println("Note: {} bytes after deflate end (ignored)", remaining);
        // }

        return decode_error::ok;
    }
} // namespace cpng
