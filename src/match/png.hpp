// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Minimal PNG writer (24 bit, stored deflate blocks) for screenshots of the new match scenes.
#include <cstdio>
#include <filesystem>
#include <vector>

#include "match/raster.hpp"

namespace em4k {

inline bool write_png(const std::filesystem::path& path, const Surface& s) {
    std::vector<uint8_t> raw;
    raw.reserve(size_t(s.height) * (size_t(s.width) * 3 + 1));
    for (int y = 0; y < s.height; y++) {
        raw.push_back(0);
        for (int x = 0; x < s.width; x++) {
            const uint32_t v = s.pixels[size_t(y) * size_t(s.width) + size_t(x)];
            raw.push_back(uint8_t(v >> 16)), raw.push_back(uint8_t(v >> 8)), raw.push_back(uint8_t(v));
        }
    }
    std::vector<uint8_t> z{0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw.size();) {
        const size_t n = std::min<size_t>(65535, raw.size() - i);
        z.push_back(i + n == raw.size());
        z.push_back(uint8_t(n)), z.push_back(uint8_t(n >> 8)), z.push_back(uint8_t(~n)), z.push_back(uint8_t(~n >> 8));
        for (size_t k = 0; k < n; k++) {
            a = (a + raw[i + k]) % 65521;
            b = (b + a) % 65521;
        }
        z.insert(z.end(), raw.begin() + ptrdiff_t(i), raw.begin() + ptrdiff_t(i + n));
        i += n;
    }
    const uint32_t adler = b << 16 | a;
    for (int k = 3; k >= 0; k--) z.push_back(uint8_t(adler >> (k * 8)));

    static uint32_t table[256];
    if (!table[1])
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
    std::vector<uint8_t> out{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
        const uint32_t n = uint32_t(data.size());
        for (int k = 3; k >= 0; k--) out.push_back(uint8_t(n >> (k * 8)));
        uint32_t crc = 0xFFFFFFFFu;
        const auto put = [&](uint8_t v) {
            out.push_back(v);
            crc = table[(crc ^ v) & 255] ^ (crc >> 8);
        };
        for (int k = 0; k < 4; k++) put(uint8_t(type[k]));
        for (uint8_t v : data) put(v);
        crc = ~crc;
        for (int k = 3; k >= 0; k--) out.push_back(uint8_t(crc >> (k * 8)));
    };
    std::vector<uint8_t> head;
    for (uint32_t v : {uint32_t(s.width), uint32_t(s.height)})
        for (int k = 3; k >= 0; k--) head.push_back(uint8_t(v >> (k * 8)));
    head.insert(head.end(), {8, 2, 0, 0, 0});
    chunk("IHDR", head);
    chunk("IDAT", z);
    chunk("IEND", {});
    FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

}  // namespace em4k
