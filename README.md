# CarrotPNG - Pure From-Scratch PNG Decoder 🥕

A clean, single-header (mostly), dependency-free PNG decoder written in modern C++23, designed specifically for game engines that want full control over asset loading.

No zlib, no libpng, no stb_image - just pure, readable code with from-scratch inflate (RFC 1951 fixed + dynamic Huffman, LZ77 window), CRC32, chunk parsing, filtering reversal, Adam7 deinterlacing, and RGBA8 output.

Built to integrate seamlessly with the Carrot Game Engine's asset/hot-reload system, custom allocators, and CarrotHLM math types (future SIMD de-filtering paths).

### Features (Current & Planned)

**MVP (in progress):**
- Full PNG signature + chunk parsing with CRC32 validation
- IHDR parsing + strict validation (color types, bit depths, compression/filter/interlace)
- Collection of IDAT chunks (ready for inflate)
- Error reporting with detailed codes

**Phase 1 Goals (Month 4–6 Carrot engine milestone):**
- From-scratch deflate/inflate implementation (no external inflate!)
- Scanline de-filtering (sub, up, average, Paeth)
- Color type expansion -> always 8-bit straight RGBA8 output
- tRNS transparency handling
- Palette (indexed) -> RGBA
- Grayscale / grayscale+alpha -> RGBA
- Adam7 interlacing full support
- Basic metadata exposure (sRGB flag, gamma, pHYs ppm)

**Future Polish:**
- Allocator-aware loading (pass your own arena)
- Hot-reload callbacks / partial reload support
- SIMD-accelerated de-filtering using CarrotHLM vector types
- Streaming/progressive decode (for very large textures)
- Optional 16-bit support

### Why CarrotPNG?

- **Zero runtime dependencies** - matches Carrot engine philosophy
- **Full control** - own the code, debug easily, extend for hot-reload
- **Educational & fun** - inflate was written from scratch for maximum dopamine
- **Carrot-branded** - namespace `cpng`, feels like home alongside `chlm`
- **Lean & safe** - modern C++23 (`[[nodiscard]]`, `noexcept`, `std::span`, uniform init), no raw pointers where avoidable
- **Tested correctness** - aiming for pixel-perfect output vs reference PNG suite

### Requirements
- **Compiler**: Clang 15+ recommended (C++23 features, consistent vector extensions if we add SIMD later)
- GCC 12+ should work; MSVC untested (but recommend clang-cl)
- C++23 standard library (`<span>`, `<optional>`, `<vector>`, etc.)
### Integration (Git Submodule Recommended)

```bash
git submodule add https://github.com/YOUR_USERNAME/CarrotPNG deps/CarrotPNG
```

In your CMakeLists.txt:
```cmake
add_subdirectory(deps/CarrotPNG)
target_link_libraries(your_engine PRIVATE CarrotPNG::CarrotPNG)
```

Then simply:
```cpp
#include <cpng/CarrotPNG.h>

using namespace cpng;

image_view_t view{};
std::vector<u8> pixel_storage;

auto err = load_from_file("assets/textures/awesome.png", view, pixel_storage);
if (err == decode_error::ok)
{
    // upload view.pixels (RGBA8, row-major) to Vulkan texture
}
```
### Current Status
- Chunk parser + CRC32 table + signature check working
- IHDR parsing & basic validation implemented
- IDAT spans collected
- Next: full inflate implementation (bit reader → fixed Huffman → dynamic Huffman → LZ77)

CarrotPNG: Because your engine deserves a PNG loader that speaks Carrot. 🥕

