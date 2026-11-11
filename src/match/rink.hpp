// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// The arena of the match scenes: ice, lines, creases, goals, boards with advertising, benches, stands and spectators,
// drawn as own vector graphics in the coordinates of the original rink picture (672 x 1152, tor_script.hpp). Only the
// layout of the ice is measured from the original; everything else is an own design.
#include <vector>

#include "match/raster.hpp"

namespace em4k {

constexpr double kRinkWidth = 672, kRinkHeight = 1152;
constexpr double kRinkMidX = 335.75, kRinkMidY = 575.75;
constexpr double kGoalLineFar = 128.5, kGoalLineNear = 1023;  // y of the two goal lines
constexpr double kGoalHalfWidth = 25, kGoalDepth = 22;

class RinkArt {
public:
    // level 0..4: how full the stands are (the arena argument of the original scene). home / away: shirt colours, for
    // the supporters, the logo at centre ice and the players on the benches.
    RinkArt(int level, Rgb home, Rgb away);

    // The arena without the goals, as far as the painter's target shows it. with_crowd = false leaves the spectators
    // out; paint_crowd then draws them moving.
    void paint(Painter& p, bool with_crowd = true) const;
    // The spectators after a goal: those who are happy about it jump. time: seconds since the goal.
    void paint_crowd(Painter& p, double time, bool home_scored) const;
    // goal 0 = far, 1 = near. bulge: how far the puck pushes the back of the net out at bulge_x (rink units).
    static void paint_goal(Painter& p, int goal, double bulge_x = kRinkMidX, double bulge = 0);

    int spectators() const { return int(crowd_.size()); }

private:
    struct Item {
        enum Kind : uint8_t { Circle, Capsule, Box, Ring } kind = Circle;
        Vec a, b;             // centre / end points; Box: b = half size
        float ra = 0, rb = 0; // radii; Box: ra = corner radius, rb = angle; Ring: rb = width
        Rgb fill, line_colour;
        float alpha = 1, line = 0;
    };
    struct Spectator {
        Vec seat, facing;
        uint32_t seed = 0;
        bool fan = false;
    };
    void build_ice();
    void build_crowd(int level);
    void draw_spectator(Painter& p, const Spectator& s, double jump) const;
    void paint_benches(Painter& p) const;
    void paint_ice_items(Painter& p) const;
    template <class F>
    void each_spectator(const Painter& p, F f) const;

    std::vector<Item> ice_;  // scratches and lines on the ice
    std::vector<std::vector<uint32_t>> ice_grid_;
    std::vector<Spectator> crowd_;
    std::vector<std::vector<uint32_t>> crowd_grid_;
    Rgb home_, away_;
};

}  // namespace em4k
