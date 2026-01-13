//
// Created by Zack Shrout on 1/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace cpng {
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
    };
} // namespace cpng
