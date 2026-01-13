//
// Created by Zack Shrout on 1/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "types.h"

#include <vector>
#include <algorithm>

namespace cpng {
    /**
     * Reverses PNG scanline filtering in-place.
     * Input: contiguous filtered data (filter byte + pixels per row) × height
     * Output: same vector, but filter bytes removed, pixels reconstructed (pure RGBA/RGB)
     *
     * Supports only 8-bit truecolor (type 2) and truecolor+alpha (type 6) for MVP.
     *
     * Returns ok on success, or error code on unsupported format/filter/corruption.
     */
    [[nodiscard]] inline decode_error defilter_scanlines(
        std::vector<uint8_t>& data,
        const uint32_t width,
        const uint32_t height,
        const uint8_t bit_depth,
        const uint8_t color_type
    ) noexcept
    {
        if (bit_depth != 8) return decode_error::unsupported_bit_depth;
        if (color_type != 2 && color_type != 6) return decode_error::unsupported_color_type;

        const uint32_t bpp = (color_type == 6) ? 4 : 3;
        const uint32_t row_bytes = 1 + width * bpp; // filter + pixels

        if (data.size() != height * row_bytes) return decode_error::invalid_idat_stream;

        std::vector<uint8_t> prior_row(width * bpp, 0); // previous reconstructed pixels

        for (uint32_t y = 0; y < height; ++y)
        {
            uint8_t* row = data.data() + y * row_bytes;
            uint8_t filter = row[0];
            uint8_t* pixels = row + 1; // pixels start right after filter byte

            if (filter == 0) // none
            {
                // already good
            }
            else if (filter == 1) // sub
            {
                for (uint32_t x = bpp; x < width * bpp; ++x)
                {
                    pixels[x] += pixels[x - bpp];
                }
            }
            else if (filter == 2) // up
            {
                for (uint32_t x = 0; x < width * bpp; ++x)
                {
                    pixels[x] += prior_row[x];
                }
            }
            else if (filter == 3) // average
            {
                for (uint32_t x = 0; x < width * bpp; ++x)
                {
                    uint8_t left = (x >= bpp) ? pixels[x - bpp] : 0;
                    pixels[x] += static_cast<uint8_t>((left + prior_row[x]) / 2);
                }
            }
            else if (filter == 4) // paeth
            {
                for (uint32_t x = 0; x < width * bpp; ++x)
                {
                    uint8_t a = (x >= bpp) ? pixels[x - bpp] : 0;
                    uint8_t b = prior_row[x];
                    uint8_t c = (x >= bpp) ? prior_row[x - bpp] : 0;

                    int p = static_cast<int>(a) + b - c;
                    int pa = std::abs(p - static_cast<int>(a));
                    int pb = std::abs(p - static_cast<int>(b));
                    int pc = std::abs(p - static_cast<int>(c));

                    uint8_t predictor = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                    pixels[x] += predictor;
                }
            }
            else
            {
                return decode_error::unsupported_filter;
            }

            // Copy reconstructed pixels (skip filter byte) to prior_row for next row
            std::copy(pixels, pixels + width * bpp, prior_row.begin());
        }

        // Compact: remove filter bytes from the entire buffer
        std::vector<uint8_t> clean;
        clean.reserve(height * width * bpp);

        for (uint32_t y = 0; y < height; ++y)
        {
            const uint8_t* row = data.data() + y * row_bytes;
            clean.insert(clean.end(), row + 1, row + 1 + width * bpp);
        }

        data = std::move(clean);

        return decode_error::ok;
    }
} // namespace cpng
