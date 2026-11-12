// SPDX-License-Identifier: GPL-3.0-only
#include "match/player.hpp"

#include <string>

#include "match/text.hpp"

namespace em4k {
namespace {

constexpr Rgb kInk{22, 30, 46};
constexpr Rgb kPants{30, 38, 60};
constexpr Rgb kSkate{38, 42, 54};
constexpr Rgb kWood{226, 192, 132};
constexpr Rgb kTape{28, 30, 38};
constexpr Rgb kPad{242, 238, 226};
// the figure is designed 15 units wide; kSize brings it to the build of the skaters of the original
constexpr double kSize = 1.32;

double luminance(Rgb c) { return 0.3 * c.r + 0.59 * c.g + 0.11 * c.b; }

}  // namespace

TeamColours team_colours(int colour) {
    const auto dac = [](int v) { return uint8_t((v & 63) << 2 | (v & 63) >> 4); };
    TeamColours t;
    t.shirt = {dac(colour >> 6), dac(colour >> 2), dac(colour << 2)};
    const bool light = luminance(t.shirt) > 150;
    t.trim = light ? Rgb{26, 40, 84} : Rgb{244, 246, 250};
    t.helmet = light ? t.trim : Rgb{240, 243, 248};
    t.glove = light ? shade(t.shirt, 0.6) : shade(t.shirt, 0.62);
    if (luminance(t.shirt) < 40) t.glove = Rgb{70, 76, 96};
    return t;
}

void draw_puck(Painter& p, Vec ground, Vec body, Vec v) {
    const double height = length(body - ground), speed = length(v);
    p.ellipse(ground + Vec{0.9, 1.3}, 2.2, 1.9, 0, Rgb{16, 70, 120}, height > 2 ? 0.3 : 0.4);
    if (speed > 2.5) {  // streak
        const Vec back = body - v * std::min(1.6, 9 / speed);
        p.capsule(back, body, 0.3, 1.5, Rgb{255, 255, 255}, 0.55);
    }
    const double r = 1.9 + std::min(1.0, height / 30);
    p.circle(body, r, Rgb{24, 26, 34}, 1, 0.35, Rgb{6, 8, 14});
    p.circle(body + Vec{-0.5, -0.6}, r * 0.37, Rgb{110, 120, 140}, 0.85);
}

double player_height(bool goalie) { return (goalie ? 20.5 : 21.5) * kSize; }

// The figure is a small skeleton in three dimensions - across, forward and up - seen at a slant: a point `height` above
// the ice is drawn height * tilt further up the picture, the ice itself stays as it is. tilt 0 is the view straight
// down, 1 about the slant of the original's sprites.
void draw_player(Painter& target, const PlayerState& state, const TeamColours& team, double tilt) {
    Painter p = target;
    p.scale = target.scale * kSize;
    p.origin = state.pos + (target.origin - state.pos) * (1 / kSize);
    PlayerState s = state;
    s.puck = state.pos + (state.puck - state.pos) * (1 / kSize);
    const Vec f{std::cos(s.heading), std::sin(s.heading)};
    const Vec r{-f.y, f.x};
    const double angle = std::atan2(r.y, r.x);
    const auto P = [&](double lateral, double forward, double height) { return s.pos + r * lateral + f * forward - Vec{0, height * tilt}; };
    const bool g = s.goalie;
    const bool cheer = s.action == PlayerState::Action::Cheer, shoot = s.action == PlayerState::Action::Shoot;
    constexpr double L = 0.55;
    const double step = std::sin(s.stride) * s.effort;
    const double towards = f.y * tilt;  // > 0: the figure shows its front
    const int slices = tilt > 0.02 ? 5 : 1;

    // A body part with a side wall: the outline of its bottom (c0, half size h0) swept up to its top (c1, h1)
    const auto solid = [&](Vec c0, Vec c1, Vec h0, Vec h1, double roundness, double turn, Rgb colour) {
        const double ca = std::cos(turn), sa = std::sin(turn);
        const double top_round = std::min(h1.x, h1.y) * roundness;
        if (slices > 1) {  // the wall: all levels in one shape, darker than the top
            const auto sweep = [=](Vec q) {
                double d = 1e9;
                for (int i = 0; i < slices; i++) {
                    const double t = i / double(slices - 1);
                    const Vec h = lerp(h0, h1, t);
                    d = std::min(d, sd_box(q, lerp(c0, c1, t), h.x, h.y, std::min(h.x, h.y) * roundness, ca, sa));
                }
                return d;
            };
            const double reach = std::max(length(h0), length(h1));
            p.shape(std::min(c0.x, c1.x) - reach, std::min(c0.y, c1.y) - reach, std::max(c0.x, c1.x) + reach, std::max(c0.y, c1.y) + reach, sweep,
                    shade(colour, 0.64), 1, L, kInk);
            p.box(c1, h1.x, h1.y, top_round, turn, colour);
        } else {
            p.box(c1, h1.x, h1.y, top_round, turn, colour, 1, L, kInk);
        }
    };

    // shadow on the ice, light from the upper left
    p.soft(s.pos + Vec{2.4, 3.6}, g ? 16.0 : 13.0, g ? 12.5 : 10.0, Rgb{8, 44, 92}, 0.5);

    // legs
    const double hip_z = g ? 8.0 : 9.0;
    if (g) {
        const bool save = shoot;
        for (int k = -1; k <= 1; k += 2) {  // the pads stand on the ice
            const double a = angle + (save ? k * 1.15 : k * 0.14), top_z = save ? 3.5 : 8.0;
            const Vec low = save ? P(k * 8.6, 3.6, 0) : P(k * 3.5, 5.2, 0), high = save ? P(k * 8.6, 3.6, top_z) : P(k * 3.3, 3.6, top_z);
            solid(low, high, {2.9, save ? 5.9 : 3.4}, {2.9, save ? 5.9 : 3.0}, 0.58, a, kPad);
            const Vec across{std::cos(a), std::sin(a)}, mid = lerp(low, high, 0.55);
            if (tilt > 0.02 && !save) p.line(mid - across * 2.3 + Vec{0, 2.4}, mid + across * 2.3 + Vec{0, 2.4}, 1.0, team.shirt);
            p.line(high - across * 2.2, high + across * 2.2, 1.0, team.shirt);
        }
        p.ellipse(P(0, -0.8, hip_z), 6.8, 4.2, angle, kPants, 1, L, kInk);
    } else {
        for (int k = -1; k <= 1; k += 2) {
            const double fwd = cheer ? 0 : k * step * 4.4;
            const double out = 3.0 + std::max(0.0, -k * step) * 1.3;
            const Vec foot = P(k * out, fwd - 0.6, 0);
            const double turn = k * (0.12 + 0.3 * std::max(0.0, -k * step));
            const Vec d = f * std::cos(turn) + r * std::sin(turn);
            const Vec hip = P(k * 2.6, -1.2, hip_z), knee = P(k * (out + 0.3), fwd * 0.55 + 1.6, 4.8);
            p.capsule(foot - d * 2.4, foot + d * 3.2, 1.35, 1.15, kSkate, 1, L, kInk);
            p.line(foot - d * 2.9, foot + d * 3.9, 0.42, Rgb{238, 242, 246});
            p.capsule(knee, foot - Vec{0, 1.0 * tilt}, 1.9, 1.6, team.shirt, 1, L, kInk);  // sock
            const Vec band = lerp(knee, foot, 0.5);
            p.line(band - r * 1.2, band + r * 1.2, 0.9, team.trim, 0.9);
            p.capsule(hip, knee, 2.3, 2.0, kPants, 1, L, kInk);
        }
        p.ellipse(P(0, -1.3, hip_z), 5.5, 3.7, angle, kPants, 1, L, kInk);
    }

    // hands and stick (the lower hand is the right one); the blade lies on the ice
    Vec top = P(-1.6, 5.0, 9.6), blade = P(6.4 + std::cos(s.stride) * 1.2 * s.effort, 12.6, 0);
    Vec right_hand, left_hand;
    const auto stick = [&] {
        if (g) {
            const Vec heel = P(5.2, 11.4, 0), toe = P(-1.4, 12.6, 0);
            p.capsule(right_hand + (right_hand - heel) * 0.9 + r * 1.2, right_hand, 0.6, 0.6, kWood, 1, 0.4, kInk);
            p.capsule(right_hand, heel, 1.05, 1.05, kWood, 1, 0.45, kInk);
            p.capsule(heel, toe, 1.15, 1.0, kTape, 1, 0.4, kInk);
            return;
        }
        const Vec dir = normal(blade - top);
        const Vec knob = cheer ? top - dir * 6.0 : top - dir * 2.4;
        const Vec toe = blade + normal(r * -0.92 + f * 0.25) * 5.4;
        p.capsule(knob, blade, 0.62, 0.62, kWood, 1, 0.42, kInk);
        p.capsule(blade, toe, 0.95, 0.8, kTape, 1, 0.4, kInk);
    };
    if (g) {
        right_hand = P(9.8, 4.0, 8.5), left_hand = P(-10.0, 4.4, 9.5);
    } else if (cheer) {  // both arms up, the stick in the air
        left_hand = P(-8.6, 2.6, 21), right_hand = P(8.6, 2.6, 21);
        top = right_hand, blade = P(12.0, 8.0, 29);
    } else {
        if (shoot) {
            const double t = s.swing;
            const Vec back = P(12.5, -5.0, 10), low = P(6.0, 14.0, 0), through = P(-5.0, 16.0, 6);
            blade = t < 0.5 ? lerp(back, low, t * 2) : lerp(low, through, t * 2 - 1);
        } else if (s.has_puck) {
            const Vec rel = s.puck - s.pos;
            const double d = length(rel);
            if (d > 5 && d < 21 && dot(rel, f) > -3) blade = s.puck - normal(rel) * 1.2 + r * 1.6;
        }
        left_hand = top;
        right_hand = lerp(top, blade, 0.44);
    }

    // arms
    const double sh = g ? 7.2 : 6.2, shoulder_z = g ? 13.0 : 14.0;
    const auto arms = [&] {
        const auto arm = [&](Vec shoulder, Vec hand, double bend) {
            const Vec elbow = lerp(shoulder, hand, 0.5) + r * bend - f * 0.4;
            p.capsule(elbow, hand, 1.8, 1.6, team.shirt, 1, L, kInk);
            p.capsule(shoulder, elbow, 2.1, 1.8, team.shirt, 1, L, kInk);
        };
        arm(P(-sh, 0.9, shoulder_z - 0.6), left_hand, -1.4);
        arm(P(sh, 0.9, shoulder_z - 0.6), right_hand, 1.4);
        if (g) {
            p.circle(left_hand, 3.1, kPad, 1, L, kInk);  // catching glove
            p.circle(left_hand + f * 0.5, 1.7, shade(kPad, 0.72));
            p.box(right_hand, 2.4, 3.2, 0.9, angle, kPad, 1, L, kInk);  // blocker
            p.line(right_hand - r * 2.0, right_hand + r * 2.0, 1.0, team.shirt);
        } else {
            p.circle(left_hand, 2.1, team.glove, 1, L, kInk);
            p.circle(right_hand, 2.1, team.glove, 1, L, kInk);
        }
    };

    // trunk: from the hips up to the shoulders, leaning forward
    const double hw = g ? 8.8 : 7.5, hd = g ? 5.1 : 4.6;
    const Vec waist = P(0, -1.0, hip_z + 0.8), body = P(0, shoot ? 1.5 : 0.8, shoulder_z);
    const auto trunk = [&] {
        solid(waist, body, {hw * 0.74, hd * 0.8}, {hw, hd}, 0.96, angle, team.shirt);
        const double ca = std::cos(angle), sa = std::sin(angle);
        const auto rim = [=](Vec q) { return std::max(sd_box(q, body, hw, hd, hd - 0.2, ca, sa), -sd_box(q + Vec{1.4, 1.5}, body, hw, hd, hd - 0.2, ca, sa)); };
        p.shape(body.x - hw, body.y - hw, body.x + hw, body.y + hw, rim, shade(team.shirt, 0.68));
        const auto shine = [=](Vec q) {
            return std::max(sd_box(q, body, hw - 1.1, hd - 1.1, hd - 1.3, ca, sa), -sd_box(q - Vec{1.2, 1.3}, body, hw - 1.1, hd - 1.1, hd - 1.3, ca, sa));
        };
        p.shape(body.x - hw, body.y - hw, body.x + hw, body.y + hw, shine, shade(team.shirt, 1.35), 0.7);
        for (int k = -1; k <= 1; k += 2)  // stripes on the sleeves
            p.line(body + r * (k * (hw - 2.0)) - f * (hd - 1.7), body + r * (k * (hw - 2.0)) + f * (hd - 1.7), 1.1, team.trim, 0.95);
        // number: on the back where the slant shows it, else on the shoulders
        const Vec back_top = body - f * (hd - 0.3), back_low = waist - f * (hd * 0.8 - 0.3);
        const double wall = back_low.y - back_top.y;
        if (s.number > 0 && towards < -0.1 && wall > 3.0) {
            const double cap = std::min(4.4, wall * 0.8);
            draw_text(p, std::to_string(s.number), lerp(back_top, back_low, 0.5) - Vec{0, cap / 2}, {1, 0}, {0, 1}, cap, 1, team.trim, 1.3);
        } else if (s.number > 0 && towards <= 0.1) {
            draw_text(p, std::to_string(s.number), body - f * 1.55, r, f * -1, 2.6, 1, team.trim, 1.25);
        } else if (towards > 0.1) {  // chest stripe
            const Vec front_top = body + f * (hd - 0.4), front_low = waist + f * (hd * 0.8 - 0.4);
            const Vec c = lerp(front_top, front_low, 0.45);
            p.line(c - r * (hw * 0.5), c + r * (hw * 0.5), std::min(1.4, std::max(0.0, front_low.y - front_top.y) * 0.4), team.trim, 0.95);
        }
    };

    // seen from behind the arms and the stick are beyond the trunk; seen from the front or from above they cover it
    const bool arms_first = towards < 0.12;
    if (arms_first) stick(), arms();
    trunk();
    if (!arms_first) stick(), arms();

    // helmet: a ball; visor and chin turn with the head and show more the more the figure faces the viewer
    const Vec head = P(0, shoot ? 3.6 : 2.9, g ? 17.0 : 18.2);
    const double hr = g ? 4.2 : 3.9;
    p.circle(head, hr, team.helmet, 1, L, kInk);
    p.shape(head.x - hr, head.y - hr, head.x + hr, head.y + hr, [=](Vec q) { return std::max(sd_circle(q, head, hr), -sd_circle(q, head - Vec{1.1, 1.2}, hr)); },
            shade(team.helmet, 0.66));
    const Vec crown = head - Vec{0, hr * 0.3 * tilt};
    p.line(crown - f * (hr - 1.3), crown + f * (hr - 2.2), 1.2, team.shirt, 0.95);
    const Vec n = normal(f + Vec{0, 0.9 * tilt});
    const double cut = hr * ((g ? 0.15 : 0.44) - 0.62 * towards);
    if (cut < hr * 0.72) {
        p.shape(head.x - hr, head.y - hr, head.x + hr, head.y + hr, [=](Vec q) { return std::max(sd_circle(q, head, hr - 0.45), cut - dot(q - head, n)); },
                g ? Rgb{176, 186, 200} : Rgb{44, 62, 96}, 1, 0.35, kInk);
        const double chin = cut + hr * 0.5;
        if (!g && chin < hr * 0.85)  // the face below the visor
            p.shape(head.x - hr, head.y - hr, head.x + hr, head.y + hr, [=](Vec q) { return std::max(sd_circle(q, head, hr - 0.45), chin - dot(q - head, n)); },
                    Rgb{240, 196, 150}, 1, 0.3, kInk);
        const Vec across{-n.y, n.x};
        if (g) {  // cage
            for (double k : {-1.5, 0.0, 1.5}) p.line(head + across * k + n * (cut + 0.3), head + across * (k * 0.8) + n * (hr - 0.7), 0.35, kInk);
            p.line(head - across * 2.9 + n * (cut + 1.6), head + across * 2.9 + n * (cut + 1.6), 0.35, kInk);
        } else {
            p.line(head + across * -1.6 + n * (cut + 0.9), head + across * -0.4 + n * (cut + 1.5), 0.5, Rgb{210, 228, 250}, 0.8);  // glint on the visor
        }
    }
    p.circle(head - Vec{1.2, 1.3}, 1.15, shade(team.helmet, 1.5), 0.75);
}

}  // namespace em4k
