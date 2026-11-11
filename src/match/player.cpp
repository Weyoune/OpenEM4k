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

void draw_player(Painter& target, const PlayerState& state, const TeamColours& team) {
    // the figure is designed 15 units wide; kSize brings it to the build of the original skaters
    constexpr double kSize = 1.32;
    Painter p = target;
    p.scale = target.scale * kSize;
    p.origin = state.pos + (target.origin - state.pos) * (1 / kSize);
    PlayerState s = state;
    s.puck = state.pos + (state.puck - state.pos) * (1 / kSize);
    const Vec f{std::cos(s.heading), std::sin(s.heading)};
    const Vec r{-f.y, f.x};
    const double angle = std::atan2(r.y, r.x);
    const auto W = [&](double lateral, double forward) { return s.pos + r * lateral + f * forward; };
    const bool g = s.goalie;
    const bool cheer = s.action == PlayerState::Action::Cheer, shoot = s.action == PlayerState::Action::Shoot;
    constexpr double L = 0.55;
    const double step = std::sin(s.stride) * s.effort;

    // shadow on the ice, light from the upper left
    p.soft(s.pos + Vec{2.4, 3.6}, g ? 16.0 : 13.0, g ? 12.5 : 10.0, Rgb{8, 44, 92}, 0.5);

    // legs
    if (g) {
        const bool save = shoot;
        for (int k = -1; k <= 1; k += 2) {
            const Vec c = save ? W(k * 8.6, 3.6) : W(k * 3.5, 5.2);
            const double a = angle + (save ? k * 1.15 : k * 0.14);
            p.box(c, 2.9, 5.9, 1.7, a, kPad, 1, L, kInk);
            const Vec along{-std::sin(a), std::cos(a)}, across{std::cos(a), std::sin(a)};
            for (double t : {-2.4, 2.6}) p.line(c + along * t - across * 2.2, c + along * t + across * 2.2, 1.0, team.shirt);
        }
        p.ellipse(W(0, -0.8), 6.8, 4.2, angle, kPants, 1, L, kInk);
    } else {
        for (int k = -1; k <= 1; k += 2) {
            const double fwd = cheer ? 0 : k * step * 4.4;
            const double out = 3.0 + std::max(0.0, -k * step) * 1.3;
            const Vec foot = W(k * out, fwd - 0.6);
            const double turn = k * (0.12 + 0.3 * std::max(0.0, -k * step));
            const Vec d = f * std::cos(turn) + r * std::sin(turn);
            p.capsule(W(k * 2.6, -0.6), foot, 2.0, 1.8, team.shirt, 1, L, kInk);
            p.capsule(foot - d * 2.4, foot + d * 3.2, 1.35, 1.15, kSkate, 1, L, kInk);
            p.line(foot - d * 2.9, foot + d * 3.9, 0.42, Rgb{238, 242, 246});
        }
        p.ellipse(W(0, -1.3), 5.5, 3.7, angle, kPants, 1, L, kInk);
    }

    // hands and stick (the lower hand is the right one)
    Vec top = W(-1.6, 4.6), blade = W(6.4 + std::cos(s.stride) * 1.2 * s.effort, 12.6);
    Vec right_hand, left_hand;
    if (g) {
        right_hand = W(9.8, 4.0), left_hand = W(-10.0, 4.4);
        const Vec heel = W(5.2, 11.4), toe = W(-1.4, 12.6);
        p.capsule(right_hand + (right_hand - heel) * 0.9 + r * 1.2, right_hand, 0.6, 0.6, kWood, 1, 0.4, kInk);
        p.capsule(right_hand, heel, 1.05, 1.05, kWood, 1, 0.45, kInk);
        p.capsule(heel, toe, 1.15, 1.0, kTape, 1, 0.4, kInk);
    } else {
        if (cheer) {
            left_hand = W(-9.2, 3.4), right_hand = W(9.2, 3.4);
            top = right_hand, blade = W(13.5, 14.5);
        } else {
            if (shoot) {
                const double t = s.swing;
                const Vec back = W(12.5, -5.0), low = W(6.0, 14.0), through = W(-5.0, 16.0);
                blade = t < 0.5 ? lerp(back, low, t * 2) : lerp(low, through, t * 2 - 1);
            } else if (s.has_puck) {
                const Vec rel = s.puck - s.pos;
                const double d = length(rel);
                if (d > 5 && d < 21 && dot(rel, f) > -3) blade = s.puck - normal(rel) * 1.2 + r * 1.6;
            }
            left_hand = top;
            right_hand = lerp(top, blade, 0.44);
        }
        const Vec dir = normal(blade - top);
        const Vec knob = cheer ? top - dir * 6.0 : top - dir * 2.4;
        const Vec toe = blade + normal(r * -0.92 + f * 0.25) * 5.4;
        p.capsule(knob, blade, 0.62, 0.62, kWood, 1, 0.42, kInk);
        p.capsule(blade, toe, 0.95, 0.8, kTape, 1, 0.4, kInk);
    }

    // arms
    const double sh = g ? 7.2 : 6.2;
    const auto arm = [&](Vec shoulder, Vec hand, double bend) {
        const Vec elbow = lerp(shoulder, hand, 0.5) + r * bend - f * 0.4;
        p.capsule(elbow, hand, 1.8, 1.6, team.shirt, 1, L, kInk);
        p.capsule(shoulder, elbow, 2.1, 1.8, team.shirt, 1, L, kInk);
    };
    arm(W(-sh, 0.9), left_hand, -1.4);
    arm(W(sh, 0.9), right_hand, 1.4);
    if (g) {
        p.circle(left_hand, 3.1, kPad, 1, L, kInk);  // catching glove
        p.circle(left_hand + f * 0.5, 1.7, shade(kPad, 0.72));
        p.box(right_hand, 2.4, 3.2, 0.9, angle, kPad, 1, L, kInk);  // blocker
        p.line(right_hand - r * 2.0, right_hand + r * 2.0, 1.0, team.shirt);
    } else {
        p.circle(left_hand, 2.1, team.glove, 1, L, kInk);
        p.circle(right_hand, 2.1, team.glove, 1, L, kInk);
    }

    // shoulders
    const Vec body = W(0, shoot ? 1.1 : 0.4);
    const double hw = g ? 8.8 : 7.5, hd = g ? 5.1 : 4.6;
    p.box(body, hw, hd, hd - 0.2, angle, team.shirt, 1, 0.6, kInk);
    {
        const double ca = std::cos(angle), sa = std::sin(angle);
        const auto rim = [=](Vec q) { return std::max(sd_box(q, body, hw, hd, hd - 0.2, ca, sa), -sd_box(q + Vec{1.4, 1.5}, body, hw, hd, hd - 0.2, ca, sa)); };
        p.shape(body.x - hw, body.y - hw, body.x + hw, body.y + hw, rim, shade(team.shirt, 0.68));
        const auto shine = [=](Vec q) {
            return std::max(sd_box(q, body, hw - 1.1, hd - 1.1, hd - 1.3, ca, sa), -sd_box(q - Vec{1.2, 1.3}, body, hw - 1.1, hd - 1.1, hd - 1.3, ca, sa));
        };
        p.shape(body.x - hw, body.y - hw, body.x + hw, body.y + hw, shine, shade(team.shirt, 1.35), 0.7);
    }
    for (int k = -1; k <= 1; k += 2)  // stripes on the sleeves
        p.line(body + r * (k * (hw - 2.0)) - f * (hd - 1.7), body + r * (k * (hw - 2.0)) + f * (hd - 1.7), 1.1, team.trim, 0.95);
    if (s.number > 0) draw_text(p, std::to_string(s.number), body - f * 1.55, r, f * -1, 2.6, 1, team.trim, 1.25);  // number on the back

    // helmet with the visor to the front
    const Vec head = W(0, shoot ? 3.9 : 3.1);
    const double hr = g ? 4.2 : 3.9;
    p.circle(head, hr, team.helmet, 1, L, kInk);
    p.shape(head.x - hr, head.y - hr, head.x + hr, head.y + hr, [=](Vec q) { return std::max(sd_circle(q, head, hr), -sd_circle(q, head - Vec{1.1, 1.2}, hr)); },
            shade(team.helmet, 0.66));
    p.line(head - f * (hr - 1.3), head + f * (hr - 2.2), 1.2, team.shirt, 0.95);
    const double cut = g ? 0.6 : 1.7;
    p.shape(head.x - hr, head.y - hr, head.x + hr, head.y + hr, [=](Vec q) { return std::max(sd_circle(q, head, hr - 0.45), cut - dot(q - head, f)); },
            g ? Rgb{176, 186, 200} : Rgb{44, 62, 96}, 1, 0.35, kInk);
    if (g) {  // cage
        for (double k : {-1.5, 0.0, 1.5}) p.line(head + r * k + f * 0.9, head + r * (k * 0.8) + f * (hr - 0.7), 0.35, kInk);
        p.line(head - r * 2.9 + f * 2.2, head + r * 2.9 + f * 2.2, 0.35, kInk);
    } else {
        p.line(head + r * -1.6 + f * 2.6, head + r * -0.4 + f * 3.3, 0.5, Rgb{210, 228, 250}, 0.8);  // glint on the visor
    }
    p.circle(head - Vec{1.2, 1.3}, 1.15, shade(team.helmet, 1.5), 0.75);
}

}  // namespace em4k
