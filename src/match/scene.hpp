// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// The match scenes of OpenEM4k: plays the scene scripts of Eishockey Manager (tor_script.hpp) as an own 2D ice hockey
// picture at any resolution and aspect ratio. A script only says where the puck and the 12 objects are per tick and
// which sprite the original showed; from that SceneTrack derives for every skater a heading (movement, the way the
// sprite faced, the puck), a stride and an action, a calm camera with a slight zoom towards the goal, spray where a
// skater brakes, the moment the puck hits the net and who shot. MatchScene draws it with own figures (player.hpp) on an
// own arena (rink.hpp).
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "match/player.hpp"
#include "match/raster.hpp"
#include "match/rink.hpp"
#include "match/tor_script.hpp"

namespace em4k {

// What a scene shows. The first block is what the original's scene takes as well.
struct MatchSceneInput {
    std::string script;                // file in TORE\, e.g. "3.at": number, ".h" / ".a" (home / away attacks), "t" goal / "v" miss
    int arena = 2;                     // 1..4: how full the stands are, anything else: the emptiest
    std::array<int, 2> colour{};       // 0xRGB shirt colours of home and away
    std::array<std::string, 2> names;  // UTF-8
    std::array<int, 2> goals{};        // the score shown
    std::string info;                  // e.g. "4500 ZUSCHAUER"
    int minute = 1;
    int frame = 0;                     // script frames played
    int side = 0;                      // who attacks: 0 home, 1 away
    bool goal = false;                 // the chance is a goal
    bool final = false;                // the script is over: the outcome is shown
    // additions of the new scenes
    double after = 0;                  // seconds since the script ended (the crowd celebrates, the net settles)
    std::string scorer;                // shown over the scorer after a goal; empty: nothing
};

// Where the picture shows the rink: pixel = (rink - origin) * scale.
struct Viewport {
    double scale = 1;
    Vec origin;
    int bar = 0;  // height of the score bar at the bottom, pixels
};
constexpr int kSceneMargin = 18;  // rink units shown above and below the window of the original (the nets lie flat here)
// The original shows 320 x 200 rink pixels at `camera`. This window stays visible above the score bar whatever the
// aspect ratio; wider or taller pictures show more of the arena around it, clamped to the 672 x 1152 arena. zoom > 1
// comes closer as far as the window allows.
Viewport scene_viewport(int width, int height, Vec camera, double zoom = 1);

// One object of a script at a (fractional) tick
struct SceneObject {
    enum class Kind : uint8_t { None, Skater, Dot } kind = Kind::None;
    int team = 0;  // 0 home, 1 away
    PlayerState state;
};
struct SceneMoment {
    Vec camera;       // top left of the original's window: calmed, or as in the script
    double zoom = 1;
    Vec puck, puck_velocity;
    std::array<SceneObject, kTorObjects> objects;
};
// Ice thrown up where a skater brakes
struct SceneSpray {
    int tick = 0;
    Vec pos, direction;
};

// A script prepared for playing.
class SceneTrack {
public:
    SceneTrack() = default;
    // attacking: 0 home, 1 away (the goalie belongs to the other team)
    SceneTrack(const TorScene& script, int attacking);
    int ticks() const { return int(moments_.size()) - 1; }  // = frames of the script
    // smooth = false: the ticks, camera and size of the script as they are
    SceneMoment at(double tick, bool smooth) const;
    const std::vector<SceneSpray>& sprays() const { return sprays_; }
    int net_tick() const { return net_tick_; }  // first tick with the puck in a net, -1: never
    int net_goal() const { return net_goal_; }  // 0 far, 1 near
    int scorer() const { return scorer_; }      // object that shot, -1: unknown

private:
    std::vector<SceneMoment> moments_;
    std::vector<Vec> script_camera_;
    std::vector<SceneSpray> sprays_;
    int net_tick_ = -1, net_goal_ = 0, scorer_ = -1;
};

class MatchScene {
public:
    explicit MatchScene(const std::filesystem::path& data);  // the directory with TORE\ in it
    int length(const std::string& script);                   // frames of TORE\<script>
    // The scripts in data\TORE, lower case and in order; empty when there is no such folder. Names of the folder and
    // of the files are matched without regard to case.
    static std::vector<std::string> scripts(const std::filesystem::path& data);
    // Draws the scene `fraction` (0..1) of a tick after scene.frame.
    void render(Surface& out, const MatchSceneInput& scene, double fraction = 0, bool smooth = true);
    // The same from a script given directly (tests, tools)
    void render(Surface& out, const SceneTrack& track, const MatchSceneInput& scene, double fraction = 0, bool smooth = true);

private:
    using Tiles = std::map<std::pair<int, int>, Surface>;
    const SceneTrack& track(const std::string& script);
    void prepare(Tiles& tiles, bool with_crowd, long long x0, long long y0, int width, int height);
    static void copy_tiles(const Tiles& tiles, Surface& out, long long x0, long long y0, int from, int to);
    void bar(Surface& out, const Viewport& view, const MatchSceneInput& scene);

    std::filesystem::path data_;
    std::map<std::string, SceneTrack> tracks_;
    std::unique_ptr<RinkArt> rink_;
    int rink_level_ = -1;
    std::array<int, 2> rink_colour_{-1, -1};
    double tile_scale_ = 0;
    Tiles tiles_[2];   // without / with the spectators
    Surface scratch_;  // the arena at tile scale under a zoomed picture
};

}  // namespace em4k
