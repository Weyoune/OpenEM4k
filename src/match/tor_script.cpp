// SPDX-License-Identifier: GPL-3.0-only
#include "match/tor_script.hpp"

#include <cstdio>
#include <cstring>

namespace em4k {
namespace {

constexpr size_t kHeader = 0x54, kFrame = 28;

struct Move {
    int dx, dy;
};
Move decode_move(uint8_t m) {
    Move r{(m >> 3) & 7, m & 7};
    if (m & 0x40) r.dx = -r.dx;
    if (m & 0x80) r.dy = -r.dy;
    return r;
}
int clamp_move(int pos, int d, int max) {
    if (d > 0) return pos + d < max ? pos + d : max;
    if (d < 0) return pos + d > 0 ? pos + d : 0;
    return pos;
}

}  // namespace

std::vector<uint8_t> read_bytes(const std::filesystem::path& path) {
    FILE* f = std::fopen(path.string().c_str(), "rb");
    if (!f) throw std::runtime_error("cannot open " + path.string());
    std::vector<uint8_t> out;
    uint8_t buffer[4096];
    for (size_t n; (n = std::fread(buffer, 1, sizeof buffer, f)) > 0;) out.insert(out.end(), buffer, buffer + n);
    std::fclose(f);
    return out;
}

TorScene decode_tor(const std::vector<uint8_t>& d) {
    if (d.size() < kHeader + 2 || std::memcmp(d.data(), "TOR!", 4) != 0) throw std::runtime_error("not a TOR! script");
    const auto be16 = [&](size_t o) { return int(d[o] << 8 | d[o + 1]); };
    TorScene s;
    s.camera_x = be16(4), s.camera_y = be16(6), s.puck_x = be16(8), s.puck_y = be16(10);
    for (size_t i = 0; i < kTorObjects; i++) s.objects[i] = {d[0x0C + i * 6 + 1], be16(0x0C + i * 6 + 2), be16(0x0C + i * 6 + 4)};
    for (size_t p = kHeader;; p += kFrame) {
        if (p + 2 > d.size()) throw std::runtime_error("TOR script without end mark");
        if (d[p] == 0xAA && d[p + 1] == 0xAA) break;
        if (p + kFrame + 2 > d.size()) throw std::runtime_error("TOR script without end mark");
        TorFrame f;
        f.camera_move = d[p], f.puck_move = d[p + 1], f.sound = d[p + 3];
        for (size_t i = 0; i < kTorObjects; i++) f.sprite[i] = d[p + 4 + i * 2], f.move[i] = d[p + 5 + i * 2];
        s.frames.push_back(f);
    }
    return s;
}

TorState tor_state_at(const TorScene& s, int n) {
    TorState v{s.camera_x > 16 ? s.camera_x - 16 : 0, s.camera_y > 32 ? s.camera_y - 32 : 0, s.puck_x, s.puck_y, s.objects};
    for (int i = 0; i < n && size_t(i) < s.frames.size(); i++) {
        const TorFrame& f = s.frames[size_t(i)];
        const Move c = decode_move(f.camera_move), p = decode_move(f.puck_move);
        v.camera_x = clamp_move(v.camera_x, c.dx, kTorCameraMaxX);
        v.camera_y = clamp_move(v.camera_y, c.dy, kTorCameraMaxY);
        v.puck_x += p.dx, v.puck_y += p.dy;
        for (size_t o = 0; o < kTorObjects; o++) {
            v.objects[o].sprite = f.sprite[o];
            if (f.sprite[o] == 0 && f.move[o] == 0) continue;
            const Move m = decode_move(f.move[o]);
            v.objects[o].x += m.dx, v.objects[o].y += m.dy;
        }
    }
    return v;
}

TorSpriteRef tor_sprite_ref(int type) {
    using K = TorSpriteRef::Kind;
    if (type == 0) return {K::Dot, 0, 0};
    if (type == 1) return {K::Puck, 0, 0};
    if (type < 0 || type > 0x5D) return {};
    if (type < 0x2A) return {K::Sheet, 0, type - 2};
    if (type < 0x52) return {K::Sheet, 1, type - 0x2A};
    return {K::Sheet, 2, type - 0x52};
}

}  // namespace em4k
