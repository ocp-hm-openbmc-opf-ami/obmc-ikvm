// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <linux/videodev2.h>

#include <cstddef>
#include <cstdint>

inline bool hasJpegEoi(const uint8_t* data, size_t size)
{
    return data != nullptr && size >= 2 && data[size - 2] == 0xFF &&
           data[size - 1] == 0xD9;
}

inline bool hasJpegEoi(const char* data, size_t size)
{
    return hasJpegEoi(reinterpret_cast<const uint8_t*>(data), size);
}

uint32_t crc32SkipJfifHeader(const char* data, size_t size, size_t skip = 0x30);
v4l2_rect clipRect(const v4l2_rect& rect, int frameWidth, int frameHeight);
