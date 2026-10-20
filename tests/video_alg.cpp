// SPDX-License-Identifier: Apache-2.0

#include "video_alg.hpp"

#include <algorithm>

namespace
{
uint32_t reflect(uint32_t value, int bits)
{
    uint32_t reflected = 0;
    for (int bit = 0; bit < bits; ++bit)
    {
        reflected = (reflected << 1) | (value & 1);
        value >>= 1;
    }
    return reflected;
}
} // namespace

uint32_t crc32SkipJfifHeader(const char* data, size_t size, size_t skip)
{
    if (data == nullptr || skip >= size)
    {
        return 0;
    }

    constexpr uint32_t polynomial = 0x04C11DB7;
    uint32_t crc = 0xFFFFFFFF;
    const auto* payload = reinterpret_cast<const unsigned char*>(data + skip);

    for (size_t index = 0; index < size - skip; ++index)
    {
        const auto byte = static_cast<uint8_t>(reflect(payload[index], 8));
        crc ^= static_cast<uint32_t>(byte) << 24;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 0x80000000U) != 0 ? (crc << 1) ^ polynomial : crc << 1;
        }
    }

    return reflect(crc, 32) ^ 0xFFFFFFFF;
}

v4l2_rect clipRect(const v4l2_rect& rect, int frameWidth, int frameHeight)
{
    v4l2_rect clipped = rect;
    clipped.left = std::max(0, std::min(clipped.left, frameWidth));
    clipped.top = std::max(0, std::min(clipped.top, frameHeight));

    const auto maxWidth =
        clipped.left >= frameWidth
            ? 0U
            : static_cast<unsigned int>(frameWidth - clipped.left);
    const auto maxHeight =
        clipped.top >= frameHeight
            ? 0U
            : static_cast<unsigned int>(frameHeight - clipped.top);
    clipped.width = std::min(clipped.width, maxWidth);
    clipped.height = std::min(clipped.height, maxHeight);
    return clipped;
}
