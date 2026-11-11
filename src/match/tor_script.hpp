// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Reader of the match scene scripts of Eishockey Manager (TORE\<n>.<h|a><t|v>[e|j]): where the puck and twelve objects
// are in every tick and which sprite the original showed for them.
//
//   0x00  "TOR!"
//   0x04  u16be camera x, y        0x08  u16be puck x, y
//   0x0C  12 x { u8 unused, u8 sprite, u16be x, u16be y }
//   0x54  frames of 28 bytes up to the u16 0xAAAA: camera move, puck move, 0, sound, 12 x { sprite, move }
//   move byte: dx = (m >> 3) & 7, dy = m & 7, bit 0x40 negates dx, bit 0x80 negates dy
// Coordinates are pixels of the original rink picture, 672 x 1152, the goals at the top and at the bottom.
#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace em4k {

constexpr int kTorObjects = 12;
constexpr int kTorViewWidth = 320, kTorViewHeight = 200;   // the window the original shows
constexpr int kTorCameraMaxX = 0x160, kTorCameraMaxY = 0x3B8;
constexpr int kTorAnchorX = 0x50, kTorAnchorY = 0x60;      // a sprite's top left corner is (x - 0x50, y - 0x60)

struct TorObject {
    int sprite = 0;
    int x = 0, y = 0;
};
struct TorFrame {
    uint8_t camera_move = 0, puck_move = 0, sound = 0;
    std::array<uint8_t, kTorObjects> sprite{}, move{};
};
struct TorScene {
    int camera_x = 0, camera_y = 0, puck_x = 0, puck_y = 0;
    std::array<TorObject, kTorObjects> objects{};
    std::vector<TorFrame> frames;
};
struct TorState {
    int camera_x = 0, camera_y = 0, puck_x = 0, puck_y = 0;
    std::array<TorObject, kTorObjects> objects{};
};
// What an object type stands for: the small dot (0), the puck's place (1), or sprite `index` of sheet 0 (home
// skaters, types 2..0x29), 1 (away skaters, 0x2A..0x51) or 2 (goalies, 0x52..0x5D).
struct TorSpriteRef {
    enum class Kind { None, Dot, Puck, Sheet } kind = Kind::None;
    int sheet = 0, index = 0;
};

std::vector<uint8_t> read_bytes(const std::filesystem::path& path);
TorScene decode_tor(const std::vector<uint8_t>& data);  // throws std::runtime_error
// The picture after n frames: the camera starts at the script's place minus (16, 32) and stays inside
// 0..0x160 / 0..0x3B8; an object whose frame entry is zero keeps its place.
TorState tor_state_at(const TorScene& scene, int n);
TorSpriteRef tor_sprite_ref(int type);

}  // namespace em4k
