// SPDX-License-Identifier: GPL-3.0-only
// Match scenes (src/match): script reader, view rules, lettering, the prepared track and rendered pictures of a script
// made up here; with the directory of the original game as the first argument also every original script.
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "match/scene.hpp"
#include "match/text.hpp"

using namespace em4k;
namespace fs = std::filesystem;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b, double eps = 1e-6) { return std::abs(a - b) < eps; }

// A chance at the near goal: the puck runs from (335.5, 900.5) straight into the net, a home skater follows it for
// eight frames and stops, an away skater and the goalie stand.
std::vector<uint8_t> made_up_script() {
    std::vector<uint8_t> d{'T', 'O', 'R', '!'};
    const auto be16 = [&](int v) { d.push_back(uint8_t(v >> 8)), d.push_back(uint8_t(v)); };
    be16(192), be16(902);  // camera: the player starts at (176, 870)
    be16(413), be16(994);  // puck
    const int objects[12][3] = {{0x0B, 400, 980}, {0x2A + 12, 460, 1040}, {0x52, 400, 1096}};
    for (const auto& o : objects) d.push_back(0), d.push_back(uint8_t(o[0])), be16(o[1]), be16(o[2]);
    for (int f = 0; f < 30; f++) {
        d.push_back(0);                 // camera
        d.push_back(f < 20 ? 7 : 0);    // puck: 7 down
        d.push_back(0), d.push_back(0);
        for (int o = 0; o < 12; o++) {
            d.push_back(uint8_t(objects[o][0]));
            d.push_back(o == 0 && f < 8 ? 5 : 0);
        }
    }
    d.push_back(0xAA), d.push_back(0xAA);
    return d;
}

void script_reader() {
    const TorScene s = decode_tor(made_up_script());
    require(s.frames.size() == 30 && s.camera_x == 192 && s.puck_y == 994, "header and frames");
    const TorState start = tor_state_at(s, 0), end = tor_state_at(s, 30);
    require(start.camera_x == 176 && start.camera_y == 870, "camera starts 16 / 32 before the script's place");
    require(end.puck_y == 994 + 140 && end.objects[0].y == 980 + 40 && end.objects[1].y == 1040, "moves add up");
    require(tor_sprite_ref(0).kind == TorSpriteRef::Kind::Dot && tor_sprite_ref(1).kind == TorSpriteRef::Kind::Puck, "small objects");
    require(tor_sprite_ref(0x29).sheet == 0 && tor_sprite_ref(0x29).index == 39 && tor_sprite_ref(0x2A).sheet == 1 && tor_sprite_ref(0x5D).index == 11, "sheets");
    require(tor_sprite_ref(0x5E).kind == TorSpriteRef::Kind::None, "nothing above 0x5D");
    bool threw = false;
    try {
        decode_tor({'T', 'O', 'R', '?'});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw, "bad script refused");
}

void viewports() {
    // 16:9: the 320 x 200 window of the original plus the margin above the bar, more rink to the sides
    Viewport v = scene_viewport(1920, 1080, {176, 400});
    require(v.bar == 135, "bar height");
    require(near(v.scale, (1080 - 135) / 236.0), "16:9 scale");
    const double wide = 1920 / v.scale, above = (1080 - v.bar) / v.scale;
    require(wide > 320 && near(above, 236), "16:9 shows the whole original window");
    require(std::abs(v.origin.x + wide / 2 - (176 + 160)) < 1 / v.scale, "window centred across");
    require(std::abs(v.origin.y + above / 2 - (400 + 100)) < 1 / v.scale, "window centred above the bar");
    require(near(v.origin.x * v.scale, std::round(v.origin.x * v.scale)), "origin on whole pixels");
    // closer: the window of the original still fits
    const Viewport z = scene_viewport(1920, 1080, {176, 400}, 1.07);
    require(near(z.scale, v.scale * 1.07) && (1080 - z.bar) / z.scale >= 200 && 1920 / z.scale >= 320, "zoom");
    // a narrow picture is limited by its width and cannot come closer
    v = scene_viewport(1000, 1600, {176, 400}, 1.07);
    require(near(v.scale, 1000 / 320.0), "portrait scale without zoom");
    // the view never leaves the arena ...
    v = scene_viewport(1920, 1080, {0, 0});
    require(v.origin.x >= 0 && v.origin.y >= 0, "clamped at the far corner");
    v = scene_viewport(1920, 1080, {352, 952}, 1.05);
    require(v.origin.x + 1920 / v.scale <= kRinkWidth + 1e-9 && v.origin.y + 1080 / v.scale <= kRinkHeight + 1e-9, "clamped at the near corner");
    // ... and a picture wider than the arena is centred
    v = scene_viewport(5120, 1080, {176, 400});
    require(5120 / v.scale > kRinkWidth && std::abs(v.origin.x + 5120 / v.scale / 2 - kRinkWidth / 2) < 1 / v.scale, "ultra wide centred");
}

void lettering() {
    require(text_width("", 12) == 0, "empty text");
    require(near(text_width("AB", 6), 4 + 1.7 + 4), "two capitals");
    require(near(text_width("ab", 6), text_width("AB", 6)), "lower case as capitals");
    require(near(text_width("KÖLN", 6), text_width("KOLN", 6)), "umlaut as wide as its letter");
    require(near(text_width("ß", 6), text_width("SS", 6)), "sharp s");
    Surface s(200, 40, Rgb{0, 0, 0});
    draw_text(s, "TOR 3:2", 100, 10, 20, 1, Rgb{255, 255, 255});
    int lit = 0;
    for (uint32_t p : s.pixels) lit += (p & 0xFF) > 128;
    require(lit > 300 && lit < 4000, "text painted");
}

void colours() {
    const TeamColours white = team_colours(0xFFF), blue = team_colours(0x00A);
    require(white.shirt.r == 255 && white.shirt.g == 255 && white.shirt.b == 243, "0xFFF is white (the DAC gets blue << 2)");
    require(blue.shirt.r == 0 && blue.shirt.g < 20 && blue.shirt.b > 150, "0x00A is blue");
    require(white.trim.b < 128 && blue.trim.b > 200, "trim contrasts with the shirt");
}

// What every prepared script must hold
void check_track(const TorScene& scene, const SceneTrack& track) {
    require(track.ticks() == int(scene.frames.size()), "one moment per frame");
    for (int n = 0; n <= track.ticks(); n++) {
        const SceneMoment whole = track.at(n, true), half = track.at(n + 0.5, true), stepped = track.at(n + 0.5, false);
        const TorState state = tor_state_at(scene, n);
        require(near(stepped.camera.x, state.camera_x) && near(stepped.camera.y, state.camera_y) && stepped.zoom == 1, "ticks show the script's camera");
        require(length(whole.camera - stepped.camera) < 24, "the calm camera stays with the script's");
        require(whole.zoom >= 1 && whole.zoom <= 1.0701, "zoom range");
        for (size_t o = 0; o < kTorObjects; o++) {
            const SceneObject& a = whole.objects[o];
            if (a.kind != SceneObject::Kind::Skater) continue;
            require(std::isfinite(a.state.heading) && std::isfinite(a.state.stride), "finite state");
            // the figure stands where the sprite of the original stood
            require(near(a.state.pos.x, state.objects[o].x - kTorAnchorX + 16.0) && near(a.state.pos.y, state.objects[o].y - kTorAnchorY + 26.0), "place");
            const SceneObject& h = half.objects[o];
            if (h.kind == SceneObject::Kind::Skater) require(length(h.state.pos - a.state.pos) <= 7, "half a tick moves at most half a step");
        }
    }
    require(track.scorer() >= -1 && track.scorer() < kTorObjects, "scorer is an object");
    for (const SceneSpray& s : track.sprays()) require(s.tick >= 0 && s.tick <= track.ticks() && near(length(s.direction), 1, 1e-3), "spray");
}

void made_up_scene() {
    const TorScene scene = decode_tor(made_up_script());
    const SceneTrack track(scene, 0);
    check_track(scene, track);
    require(track.net_goal() == 1 && track.net_tick() >= 17 && track.net_tick() <= 19, "the puck reaches the near net after 18 frames");
    require(track.scorer() == 0, "the home skater at the puck shot");
    require(!track.sprays().empty() && track.sprays()[0].direction.y > 0.9, "he brakes and throws up ice");
    const SceneMoment m = track.at(3, true);
    require(m.objects[0].team == 0 && m.objects[1].team == 1 && m.objects[2].state.goalie && m.objects[2].team == 1, "teams; the goalie defends");
    require(std::abs(m.objects[0].state.heading - 1.5708) < 0.3, "skating down the rink");

    MatchScene hd("");
    MatchSceneInput in;
    in.script = "made up";
    in.colour = {0x03C, 0xEEE};
    in.names = {"EHC FREIBURG", "KÖLNER EC"};
    in.goals = {3, 2};
    in.info = "AUSWÄRTS";
    in.minute = 16;
    in.frame = 6;
    in.goal = true;
    Surface a(640, 360), b(640, 360);
    hd.render(a, track, in, 0.5, true);
    hd.render(b, track, in, 0.5, true);
    require(a.pixels == b.pixels, "same picture twice");
    const Rgb ice = Surface::unpack(a.pixels[size_t(20) * 640 + 320]), bar = Surface::unpack(a.pixels[size_t(355) * 640 + 4]);
    require(ice.g > 200 && ice.b > 200 && ice.r < 215, "ice at the top");
    require(bar.r < 90 && bar.g < 90, "bar at the bottom");
    hd.render(b, track, in, 0.5, false);
    require(a.pixels != b.pixels, "the ticks of the script look different from the smooth picture");
    // the outcome: crowd, net, goal light and the scorer's name
    in.frame = 30, in.final = true, in.goals = {4, 2}, in.scorer = "BERGER";
    in.after = 0.1;
    hd.render(a, track, in);
    in.after = 0.6;
    hd.render(b, track, in);
    require(a.pixels != b.pixels, "the outcome moves");
    Surface small(64, 48), tall(300, 900), wide(1600, 400);
    hd.render(small, track, in);
    hd.render(tall, track, in);
    hd.render(wide, track, in);
    hd.render(a, SceneTrack(), in);  // the empty rink
}

void originals(const fs::path& data) {
    int scripts = 0;
    for (const auto& e : fs::directory_iterator(data / "TORE")) {
        const std::string name = e.path().filename().string();
        if (name == "anzahl") continue;
        const TorScene scene = decode_tor(read_bytes(e.path()));
        check_track(scene, SceneTrack(scene, name.find(".a") != std::string::npos));
        scripts++;
    }
    require(scripts == 48, "48 scripts");
    MatchScene hd(data);
    MatchSceneInput in;
    in.script = "5.at";
    in.frame = 38;
    Surface picture(640, 360);
    hd.render(picture, in, 0.5);
    require(hd.length("5.at") == 80, "length of 5.at");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        script_reader();
        viewports();
        lettering();
        colours();
        made_up_scene();
        if (argc > 1) originals(argv[1]);
        std::puts(argc > 1 ? "match tests passed (with the original scripts)" : "match tests passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "match tests failed: %s\n", e.what());
        return 1;
    }
}
