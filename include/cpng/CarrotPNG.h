//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

/**
 * @file CarrotPNG.h
 * @brief Lightweight PNG image decoder.
 *
 * CarrotPNG is a small, dependency-free PNG loader designed for game engines
 * and real-time applications. It focuses on predictable behavior, minimal
 * allocations, and a simple API for loading PNG images directly into RGBA8
 * pixel buffers.
 *
 * The decoder performs the full PNG pipeline:
 *  - PNG chunk parsing and validation
 *  - zlib/DEFLATE decompression of IDAT data
 *  - PNG scanline filter reconstruction
 *  - Conversion to RGBA8 pixel format
 *
 * Current supported PNG features (MVP):
 *  - Bit depth: 8
 *  - Color types:
 *      - RGB (2)
 *      - RGBA (6)
 *  - Non-interlaced images only
 *
 * Unsupported features will return an appropriate @ref decode_error.
 *
 * Typical usage:
 *
 * @code
 * #include <cpng/CarrotPNG.h>
 *
 * std::vector<uint8_t> pixels;
 * cpng::image_view_t image{};
 *
 * cpng::decode_error err =
 *     cpng::load_from_file("texture.png", image, pixels);
 *
 * if (err != cpng::decode_error::ok)
 * {
 *     std::println("PNG load failed: {}",
 *                  cpng::to_string(err));
 * }
 *
 * // Access RGBA pixels
 * const uint8_t* data = image.pixels.data();
 * uint32_t width = image.width;
 * uint32_t height = image.height;
 * @endcode
 *
 * Memory ownership:
 *  - Pixel memory is owned by the caller via the provided storage vector.
 *  - @ref image_view_t provides a lightweight view referencing that storage.
 *
 * Design goals:
 *  - No external dependencies
 *  - Simple API suitable for engine integration
 *  - Predictable memory ownership
 *  - Clear error reporting
 *
 * @note
 * This library does not perform color management or gamma correction.
 * PNG color information (gAMA, sRGB, etc.) may be supported in future
 * versions.
 *
 * @author Zack Shrout
 * @copyright BunnySoft
 */

#pragma once

#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstddef>

namespace cpng {
    struct image_view_t
    {
        uint32_t                    width{ };
        uint32_t                    height{ };
        std::span<const uint8_t>    pixels{ }; // contiguous RGBA8, row-major
        uint32_t                    stride_bytes{ };
        bool                        is_srgb{ true };
    };

    enum class decode_error : uint8_t
    {
        ok,
        invalid_signature,
        file_too_short,
        output_buffer_too_small,
        invalid_chunk_length,
        crc_mismatch,
        duplicate_ihdr,
        missing_ihdr,
        unexpected_chunk_order,
        unsupported_color_type,
        unsupported_bit_depth,
        no_idat_chunks,
        invalid_idat_stream,
        no_iend,
        unsupported_compression_filter,
        unsupported_interlace,
        unsupported_filter,
        file_not_found,
    };

    struct ihdr_info_t
    {
        uint32_t    width{ };
        uint32_t    height{ };
        uint8_t     bit_depth{ };
        uint8_t     color_type{ };
        uint8_t     compression_method{ };
        uint8_t     filter_method{ };
        uint8_t     interlace_method{ };
        bool        valid{ false };
        bool        has_srgb{ false };
        bool        has_gamma{ false };
        float       gamma{ 0.0f };
        bool        has_icc_profile{ false };
    };

    // ──────────────────────────────────────────────────────────────────────────────
    // Public API
    // ──────────────────────────────────────────────────────────────────────────────

    /**
     * @brief Reads the PNG IHDR header from a memory buffer without decoding the image.
     *
     * This function parses only the PNG signature and the first IHDR chunk in order
     * to retrieve basic image metadata such as width, height, bit depth, and color
     * type. No decompression, filtering, or pixel allocation occurs.
     *
     * This is useful when a caller only needs to inspect image properties
     * (e.g. validating dimensions or determining required memory) without
     * performing a full decode.
     *
     * @param data
     *     A contiguous memory buffer containing the PNG file contents.
     *
     * @param out_ihdr
     *     Output structure filled with the parsed IHDR information on success.
     *
     * @return
     *     - decode_error::ok on success.
     *     - decode_error::invalid_signature if the PNG signature is invalid.
     *     - decode_error::missing_ihdr if the file does not contain a valid IHDR.
     *     - decode_error::crc_mismatch if the IHDR CRC check fails.
     *     - decode_error::file_too_short if the input buffer is truncated.
     *
     * @note
     *     This function performs no dynamic memory allocations.
     */
    [[nodiscard]] decode_error read_ihdr_from_memory(std::span<const uint8_t> data, ihdr_info_t& out_ihdr) noexcept;

    /**
     * @brief Fully decodes a PNG image from memory into RGBA8 pixel data.
     *
     * This function parses the PNG structure, concatenates IDAT chunks,
     * inflates the DEFLATE stream, applies PNG scanline filters, and
     * converts the result into a contiguous RGBA8 pixel buffer.
     *
     * Supported features (current MVP implementation):
     * - Non-interlaced images only
     * - Bit depth: 8
     * - Color types: RGB (2) and RGBA (6)
     *
     * RGB images are automatically expanded to RGBA with alpha set to 255.
     *
     * @param data
     *     A contiguous memory buffer containing the PNG file contents.
     *
     * @param out_view
     *     Receives a lightweight view describing the decoded image.
     *     The pixel span references the storage in `out_pixel_storage`.
     *
     * @param out_pixel_storage
     *     Storage buffer that will receive the decoded RGBA8 pixels.
     *     The buffer will be resized as necessary.
     *
     * @return
     *     - decode_error::ok on success.
     *     - decode_error::invalid_signature if the PNG header is invalid.
     *     - decode_error::unsupported_color_type if the image format is unsupported.
     *     - decode_error::unsupported_bit_depth if the bit depth is unsupported.
     *     - decode_error::invalid_idat_stream if decompression fails.
     *     - decode_error::unsupported_filter if an unsupported PNG filter is encountered.
     *
     * @note
     *     The returned image pixels remain valid as long as `out_pixel_storage`
     *     remains alive.
     *
     * @warning
     *     This function may allocate memory for intermediate buffers and for the
     *     final pixel storage.
     */
    [[nodiscard]] decode_error load_from_memory(std::span<const uint8_t> data, image_view_t& out_view,
                                                std::vector<uint8_t>& out_pixel_storage) noexcept;

    /**
     * @brief Loads and decodes a PNG image directly from a file on disk.
     *
     * This is a convenience wrapper around @ref load_from_memory that reads the
     * entire file into memory and then performs a normal decode.
     *
     * @param path
     *     Filesystem path to the PNG file.
     *
     * @param out_view
     *     Receives a lightweight view describing the decoded image.
     *     The pixel span references the storage in `out_pixel_storage`.
     *
     * @param out_pixel_storage
     *     Storage buffer that will receive the decoded RGBA8 pixels.
     *     The buffer will be resized as necessary.
     *
     * @return
     *     - decode_error::ok on success.
     *     - decode_error::file_not_found if the file cannot be opened.
     *     - decode_error::file_too_short if the file cannot be fully read.
     *     - Any error returned by @ref load_from_memory during decoding.
     */
    [[nodiscard]] decode_error load_from_file(const char* path, image_view_t& out_view,
                                              std::vector<uint8_t>& out_pixel_storage) noexcept;

    // ──────────────────────────────────────────────────────────────────────────────
    // Convenience / engine ergonomics helpers
    // ──────────────────────────────────────────────────────────────────────────────

    /**
     * @brief Reads the PNG IHDR header from a file without decoding the image.
     *
     * This is a convenience wrapper around @ref read_ihdr_from_memory that only reads
     * the minimum bytes required to validate the signature and IHDR chunk.
     */
    [[nodiscard]] decode_error read_ihdr_from_file(const char* path, ihdr_info_t& out_ihdr) noexcept;

    /**
     * @brief Returns the size in bytes required for an RGBA8 buffer for this image.
     *
     * This is a convenience for pre-allocation in engines.
     */
    [[nodiscard]] constexpr size_t rgba8_size_bytes(const ihdr_info_t& ihdr) noexcept
    {
        return static_cast<size_t>(ihdr.width) * static_cast<size_t>(ihdr.height) * 4u;
    }

    /**
     * @brief Fully decodes a PNG image from memory into a caller-provided RGBA8 buffer.
     *
     * The caller must provide `out_rgba8` with at least `width*height*4` bytes.
     * On success, @ref image_view_t::pixels will reference `out_rgba8`.
     */
    [[nodiscard]] decode_error load_from_memory(std::span<const uint8_t> data, image_view_t& out_view,
                                                std::span<uint8_t> out_rgba8) noexcept;

    /**
     * @brief Returns the CarrotPNG version string.
     *
     * Useful for logging and diagnostics.
     */
    [[nodiscard]] constexpr std::string_view version_string() noexcept
    {
        return "CarrotPNG 0.1";
    }

    /**
     * @brief Converts a decode_error value to a human-readable string.
     *
     * This function is primarily intended for logging, debugging,
     * and diagnostic output.
     *
     * @param err
     *     The error code to convert.
     *
     * @return
     *     A string describing the error.
     *
     * @note
     *     The returned string is a static string literal and does not allocate.
     */
    [[nodiscard]] std::string_view to_string(decode_error err) noexcept;
} // namespace cpng
