// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Skaters, goalies and the puck of the match scenes: own vector figures seen from above at a slant. A figure is
// drawn from a state (place, heading, stride, action), not from pictures, so it can turn freely and move smoothly;
// the scene player (scene.hpp) derives these states from the original scripts.
#include "match/raster.hpp"

namespace em4k {

struct TeamColours {
    Rgb shirt, trim, helmet, glove;
};
// colour: the 0xRGB word of the save (team byte +0x3B), sent to the VGA DAC as in the original
TeamColours team_colours(int colour);

struct PlayerState {
    enum class Action : uint8_t { Skate, Shoot, Cheer };
    Vec pos;               // on the ice, rink units
    double heading = 0;    // radians, direction the figure faces (cos, sin)
    double stride = 0;     // phase of the skating step, radians
    double effort = 0;     // 0 gliding .. 1 full stride
    Action action = Action::Skate;
    double swing = 0;      // Shoot: 0 wind-up .. 1 follow-through
    bool goalie = false;
    int number = 0;        // on the back, 0 = none
    bool has_puck = false; // the blade goes to `puck`
    Vec puck;
};

// tilt 0..1: how much the figure is seen from the side (0 straight from above, 1 about as in the original); the place
// on the ice does not depend on it.
void draw_player(Painter& p, const PlayerState& s, const TeamColours& team, double tilt);
// How far the top of a figure is above its place on the ice, in rink units (to be multiplied by the tilt).
double player_height(bool goalie);
// ground: where the puck is over the ice (its shadow), body: where it is seen (lifted pucks are drawn off the shadow);
// velocity in rink units per script tick, for the streak behind a fast puck
void draw_puck(Painter& p, Vec ground, Vec body, Vec velocity);

}  // namespace em4k
