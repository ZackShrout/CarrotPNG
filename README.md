# CarrotPNG 🥕

![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)
![Build](https://img.shields.io/badge/build-CMake-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Dependencies](https://img.shields.io/badge/dependencies-none-success)

## Quick Start

Add CarrotPNG to your project:

```bash
git submodule add https://github.com/ZackShrout/CarrotPNG external/CarrotPNG
```

```cmake
add_subdirectory(external/CarrotPNG)
target_link_libraries(your_engine PRIVATE CarrotPNG::CarrotPNG)
```

Decode a PNG file:

```c++
#include <cpng/CarrotPNG.h>

cpng::image_view_t image{ };
std::vector<uint8_t> pixels;

auto err{ cpng::load_from_file("texture.png", image, pixels) };

if (err == cpng::decode_error::ok)
{
    // image.width
    // image.height
    // pixels -> RGBA8
}
```

CarrotPNG outputs **straight RGBA8 pixel buffers ready for GPU upload**.

---

**CarrotPNG is a clean, dependency-free PNG decoder written entirely from scratch in modern C++23.**

CarrotPNG is a lightweight PNG decoder designed for game engines and tools that want **full control over asset loading** without relying on large external libraries.

No `libpng`  
No `zlib`  
No `stb_image`

Everything — including **DEFLATE (RFC1951)** — is implemented from scratch.

The decoder outputs **RGBA8 pixel buffers ready for GPU upload**.

Originally built for the **Carrot Game Engine**, but fully standalone and easy to integrate into any CMake project.

---

# Contents

- [Features](#features)
- [Design Goals](#design-goals)
- [Status](#status)
- [Requirements](#requirements)
- [Integration](#integration)
- [Basic Usage](#basic-usage)
- [Output Format](#output-format)
- [Repository Layout](#repository-layout)
- [Testing](#testing)
- [Why Not stb_image?](#why-not-stb_image)
- [License](#license)

---

# Features

### PNG Parsing

- PNG signature validation
- Chunk parsing with **CRC32 verification**
- Strict **IHDR validation**
- Multi-chunk **IDAT handling**

### From-Scratch DEFLATE

Complete implementation of the PNG compression pipeline:

- Bit-level reader
- Fixed Huffman decoding
- Dynamic Huffman decoding
- Canonical Huffman table builder
- LZ77 sliding window reconstruction
- Zlib header validation
- Adler-32 verification

### Image Reconstruction

- PNG scanline **de-filtering**
    - None
    - Sub
    - Up
    - Average
    - Paeth
- **Adam7 interlacing**
- Color conversion to **8-bit RGBA**

Supported input formats:

| PNG Color Type | Supported |
|----------------|-----------|
| Grayscale | ✓ |
| RGB | ✓ |
| Indexed (Palette) | ✓ |
| Grayscale + Alpha | ✓ |
| RGBA | ✓ |

### Transparency

- `tRNS` chunk support for palette, grayscale, and RGB images

---

# Design Goals

CarrotPNG follows a few simple principles.

### Zero Dependencies

No external compression libraries or image loaders.

### Engine-Friendly

Simple API, predictable allocations, and easy integration into custom asset pipelines.

### Readable Implementation

The code prioritizes **clarity and debuggability** over extreme micro-optimization.

### Modern C++

Uses modern language features including:

- `std::span`
- `std::optional`
- `[[nodiscard]]`
- `noexcept`
- `std::print`
- C++23 standard library

---

# Status

CarrotPNG currently implements the **full PNG decoding pipeline**:

- PNG signature + chunk parsing
- CRC validation
- DEFLATE (fixed + dynamic Huffman)
- LZ77 reconstruction
- scanline de-filtering
- Adam7 interlacing
- color expansion to RGBA8

The library is considered **stable for engine integration**.

Future improvements may include:

- SIMD-accelerated de-filtering
- allocator-aware decoding
- progressive/streaming decoding for large textures
- optional 16-bit support

---

# Requirements

- **CMake 3.20+**
- **C++23 compiler**

Recommended:

- Clang 15+
- GCC 12+

MSVC should work but has not been heavily tested.

---

# Integration

The easiest way to integrate CarrotPNG is with **git submodules**.

```bash
git submodule add https://github.com/YOUR_USERNAME/CarrotPNG external/CarrotPNG
```

Then in your `CMakeLists.txt`:

```cmake
add_subdirectory(external/CarrotPNG)

target_link_libraries(your_engine
    PRIVATE
    CarrotPNG::CarrotPNG
)
```

CarrotPNG exposes the CMake target:

```cmake
CarrotPNG::CarrotPNG
```

Include the header:

```c++
#include <cpng/CarrotPNG.h>
```

---
# Basic Usage

CarrotPNG exposes a **simple decode API** that reads PNG files and outputs
a contiguous RGBA8 pixel buffer ready for GPU upload.

```c++
#include <cpng/CarrotPNG.h>

cpng::image_view_t image{ };
std::vector<uint8_t> pixels;

auto err{ cpng::load_from_file(
    "assets/textures/example.png",
    image,
    pixels
) };

if (err != cpng::decode_error::ok)
{
    // handle error
}

// image.width
// image.height
// image.pixels -> RGBA8 data
```

The pixel buffer is stored in the provided vector.

---

# Output Format

All decoded images are returned as:

| Property     | Value                        |
| ------------ | ---------------------------- |
| Pixel Format | RGBA                         |
| Bit Depth    | 8-bit per channel            |
| Layout       | Row-major                    |
| Alpha        | Straight (not premultiplied) |

This makes the result directly usable for GPU uploads in APIs like:
- Vulkan
- DirectX
- Metal
- OpenGL

---

# Repository Layout

```text
CarrotPNG
├─ include/
│  └─ cpng/
│     └─ CarrotPNG.h
│
├─ src/
│  ├─ CarrotPNG.cpp
│  └─ internal/
│     ├─ bit_reader.h
│     ├─ chunk_parser.h
│     ├─ crc32.h
│     ├─ defilter.h
│     ├─ fixed_tables.h
│     ├─ huffman.h
│     └─ inflate.h
│
├─ test/
│  └─ validation suite
│
└─ CMakeLists.txt
```

---

# Testing

CarrotPNG includes a validation test executable.

To enable tests when building the library directly:

```bash
cmake -DCARROTPNG_BUILD_TESTS=ON ..
```

Then run:

```bash
ctest
```

Tests are disabled automatically when the project is included as a submodule.

---

# Why Not stb_image?

Libraries like `stb_image` are fantastic.

CarrotPNG exists for cases where you want:
- zero external dependencies
- full control over the decoder
- the ability to debug and extend the implementation
- educational value in understanding PNG and DEFLATE

The implementation aims to remain approachable and readable, making it easy to modify for custom engine needs.

---

# License

MIT License.

---

# Carrot Ecosystem

CarrotPNG is part of the growing **Carrot Ecosystem**:

- **CarrotPNG** - PNG decoding
- **CarrotHLM** - high-level math library
- **Carrot Engine** - the game engine

---

🥕 **CarrotPNG — because your engine deserves a PNG loader that speaks Carrot.**