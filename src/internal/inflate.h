//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "huffman.h"
#include "defilter.h"

#include <vector>
#include <print>

namespace cpng {
    [[nodiscard]] constexpr decode_error concat_idat(std::span<const std::span<const uint8_t>> idat_spans,
                                                     std::vector<uint8_t>& out_concat) noexcept
    {
        size_t total{ 0 };
        for (auto sp: idat_spans) total += sp.size();

        out_concat.clear();
        out_concat.reserve(total);

        for (auto sp: idat_spans)
            out_concat.insert(out_concat.end(), sp.begin(), sp.end());

        return decode_error::ok;
    }

    [[nodiscard]] constexpr decode_error inflate_idat(std::span<const uint8_t> zlib_data,
                                                      std::vector<uint8_t>& out_decompressed,
                                                      const size_t expected_size, uint32_t width, uint32_t height,
                                                      uint8_t bit_depth, uint8_t color_type) noexcept
    {
        if (zlib_data.size() < 6) return decode_error::invalid_idat_stream;

        const uint8_t cmf{ zlib_data[0] };
        const uint8_t flg{ zlib_data[1] };

        if ((cmf & 0x0F) != 8) return decode_error::invalid_idat_stream;

        uint16_t check{ static_cast<uint16_t>(static_cast<uint16_t>(cmf) << 8 | flg) };

        if (check % 31 != 0) return decode_error::invalid_idat_stream;
        if (flg & 0x20) return decode_error::invalid_idat_stream; // no dict

        std::span<const uint8_t> deflate_data{ zlib_data.begin() + 2, zlib_data.size() - 6 };

        bit_reader_t reader{ };
        reader.data = deflate_data;

        out_decompressed.clear();
        out_decompressed.reserve(expected_size);

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

            if (*btype_opt == 0) // stored (uncompressed)
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

                out_decompressed.insert(out_decompressed.end(), deflate_data.begin() + reader.byte_pos,
                                        deflate_data.begin() + reader.byte_pos + len);
                reader.byte_pos += len;
            }
            else if (*btype_opt == 1) // fixed Huffman
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
            else if (*btype_opt == 2) // dynamic Huffman
            {
                // 1. Read HLIT, HDIST, HCLEN
                auto hlit_opt{ reader.get_bits(5) };
                auto hdist_opt{ reader.get_bits(5) };
                auto hclen_opt{ reader.get_bits(4) };

                if (!hlit_opt || !hdist_opt || !hclen_opt)
                    return decode_error::invalid_idat_stream;

                const int n_lit_len{ static_cast<int>(*hlit_opt) + 257 }; // 257..286
                const int n_dist{ static_cast<int>(*hdist_opt) + 1 }; // 1..30
                const int n_clen{ static_cast<int>(*hclen_opt) + 4 }; // 4..19

                if (n_lit_len > 286 || n_dist > 30 || n_clen > 19)
                    return decode_error::invalid_idat_stream;

                // 2. Read code lengths for code-length alphabet (in weird order)
                std::array<uint8_t, 19> clen_lengths{ }; // zero-initialized

                for (int i = 0; i < n_clen; ++i)
                {
                    constexpr std::array<int, 19> clen_order{
                        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
                    };
                    auto len_opt = reader.get_bits(3);

                    if (!len_opt) return decode_error::invalid_idat_stream;

                    clen_lengths[clen_order[i]] = static_cast<uint8_t>(*len_opt);
                }

                std::array<int, 19> clen_lengths_int{ };
                std::copy(clen_lengths.begin(), clen_lengths.end(), clen_lengths_int.begin());

                // 3. Build small Huffman table for code lengths
                huffman_table_t clen_table{ build_huffman_table(clen_lengths_int.data(), 19) };

                // 4. Decode the actual lit/len + dist lengths
                std::vector<uint8_t> all_lengths(n_lit_len + n_dist, 0);
                size_t idx{ 0 };
                uint8_t prev_len{ 0 };

                while (idx < all_lengths.size())
                {
                    int sym{ huffman_decode(reader, clen_table) };
                    if (sym < 0) return decode_error::invalid_idat_stream;

                    if (sym < 16)
                    {
                        // literal length
                        all_lengths[idx++] = static_cast<uint8_t>(sym);
                        prev_len = static_cast<uint8_t>(sym);
                    }
                    else if (sym == 16)
                    {
                        if (idx == 0) return decode_error::invalid_idat_stream; // no previous

                        auto extra_opt{ reader.get_bits(2) };
                        if (!extra_opt) return decode_error::invalid_idat_stream;

                        int repeat{ 3 + static_cast<int>(*extra_opt) }; // 3..6

                        for (int r{ 0 }; r < repeat && idx < all_lengths.size(); ++r)
                            all_lengths[idx++] = prev_len;
                    }
                    else if (sym == 17)
                    {
                        auto extra_opt{ reader.get_bits(3) };
                        if (!extra_opt) return decode_error::invalid_idat_stream;

                        int repeat{ 3 + static_cast<int>(*extra_opt) }; // 3..10

                        for (int r{ 0 }; r < repeat && idx < all_lengths.size(); ++r)
                            all_lengths[idx++] = 0;

                        prev_len = 0;
                    }
                    else if (sym == 18)
                    {
                        auto extra_opt{ reader.get_bits(7) };
                        if (!extra_opt) return decode_error::invalid_idat_stream;

                        int repeat{ 11 + static_cast<int>(*extra_opt) }; // 11..138

                        for (int r{ 0 }; r < repeat && idx < all_lengths.size(); ++r)
                            all_lengths[idx++] = 0;

                        prev_len = 0;
                    }
                    else
                    {
                        return decode_error::invalid_idat_stream; // impossible
                    }
                }

                // 5. Split into lit/len and distance lengths
                std::span<const uint8_t> lit_len_lengths(all_lengths.data(), n_lit_len);
                std::span<const uint8_t> dist_lengths(all_lengths.data() + n_lit_len, n_dist);

                // Convert to int[] for build
                constexpr int MAX_LIT_LEN{ 288 };
                std::vector<int> lit_len_int(MAX_LIT_LEN, 0); // pad with 0s
                constexpr int MAX_DIST{ 30 };
                std::vector<int> dist_int(MAX_DIST, 0);

                std::copy(lit_len_lengths.begin(), lit_len_lengths.end(), lit_len_int.begin());
                std::copy(dist_lengths.begin(), dist_lengths.end(), dist_int.begin());

                // 6. Build the two tables
                huffman_table_t lit_len_table{ build_huffman_table(lit_len_int.data(), MAX_LIT_LEN) };
                huffman_table_t dist_table{ build_huffman_table(dist_int.data(), MAX_DIST) };

                // 7. Now decode using these tables — almost identical to fixed case
                int symbols_decoded{ 0 };

                while (true)
                {
                    int sym{ huffman_decode(reader, lit_len_table) };

                    if (sym < 0)
                    {
                        std::println("Decode failed at symbol {} ({} bytes so far)", symbols_decoded,
                                     out_decompressed.size());
                        return decode_error::invalid_idat_stream;
                    }

                    if (sym < 256)
                    {
                        out_decompressed.push_back(static_cast<uint8_t>(sym));
                    }
                    else if (sym == 256)
                    {
                        break;
                    }
                    else // length code 257–285
                    {
                        const int index{ sym - 257 };
                        if (index >= 29)
                        {
                            std::println("Invalid length code {} at symbol {}", sym, symbols_decoded);
                            return decode_error::invalid_idat_stream;
                        }

                        int len{ length_base[index] };
                        if (length_extra[index] > 0)
                        {
                            auto ex_opt{ reader.get_bits(length_extra[index]) };
                            if (!ex_opt)
                            {
                                std::println("Length extra underflow at symbol {} (need {} bits)", symbols_decoded,
                                             length_extra[index]);
                                return decode_error::invalid_idat_stream;
                            }
                            len += static_cast<int>(*ex_opt);
                        }

                        // Distance
                        int dist_sym{ huffman_decode(reader, dist_table) };
                        if (dist_sym < 0 || dist_sym >= 30)
                        {
                            std::println("Distance decode failed after length at symbol {}", symbols_decoded);
                            return decode_error::invalid_idat_stream;
                        }

                        int dist{ dist_base[dist_sym] };
                        if (dist_extra[dist_sym] > 0)
                        {
                            auto ex_opt{ reader.get_bits(dist_extra[dist_sym]) };
                            if (!ex_opt)
                            {
                                std::println("Distance extra underflow after length at symbol {}", symbols_decoded);
                                return decode_error::invalid_idat_stream;
                            }
                            dist += static_cast<int>(*ex_opt);
                        }

                        // Safety: distance too large
                        if (dist > static_cast<int>(out_decompressed.size()))
                        {
                            std::println("Invalid distance {} > current size {} at symbol {}", dist,
                                         out_decompressed.size(), symbols_decoded);
                            return decode_error::invalid_idat_stream;
                        }

                        for (int i{ 0 }; i < len; ++i)
                        {
                            const size_t src_pos = out_decompressed.size() - static_cast<size_t>(dist);
                            out_decompressed.push_back(out_decompressed[src_pos]);
                        }
                    }

                    symbols_decoded++;
                }
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

        // Adler-32 verification (source of truth)
        if (zlib_data.size() < 4) return decode_error::invalid_idat_stream;

        const uint32_t adler_expected{
            static_cast<uint32_t>(zlib_data[zlib_data.size() - 4]) << 24 |
            static_cast<uint32_t>(zlib_data[zlib_data.size() - 3]) << 16 |
            static_cast<uint32_t>(zlib_data[zlib_data.size() - 2]) << 8 |
            static_cast<uint32_t>(zlib_data[zlib_data.size() - 1])
        };

        uint32_t s1{ 1 };
        uint32_t s2{ 0 };
        for (const uint8_t b: out_decompressed)
        {
            s1 = (s1 + b) % 65521;
            s2 = (s2 + s1) % 65521;
        }

        if (const uint32_t adler_computed{ s2 << 16 | s1 }; adler_computed != adler_expected)
        {
            std::println(stderr, "Alder-32 mismatch: computed={}, expected={}", adler_computed, adler_expected);
            return decode_error::invalid_idat_stream;
        }

        // Final size handling
        if (out_decompressed.size() < expected_size)
        {
            std::println(stderr, "Underrun: got {}, expected {}", out_decompressed.size(), expected_size);
            return decode_error::invalid_idat_stream;
        }

        if (out_decompressed.size() > expected_size)
        {
            out_decompressed.resize(expected_size);
        }

        const decode_error defilter_err{ defilter_scanlines(out_decompressed, width, height, bit_depth, color_type) };

        if (defilter_err != decode_error::ok) return defilter_err;

        return decode_error::ok;
    }
} // namespace cpng
