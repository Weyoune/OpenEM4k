// SPDX-License-Identifier: GPL-3.0-only
#include "match/scene.hpp"

#include <thread>

#include "match/text.hpp"

namespace em4k {
namespace {

constexpr double kPi = 3.14159265358979;
constexpr double kTickSeconds = 0.054925;  // one BIOS tick per script frame
constexpr int kTile = 256;

// What the 40 skater sprites of the original show: the way the figure faces - Up the rink, Down, Left, Right - and
// whether it swings or lifts the stick (sprites 21..29, 36, 37). The 12 goalie sprites face up (0..5, the goalie of the
// near goal seen from behind) or down (6..11).
constexpr char kFacing[41] = "UUUUUURRRDDDDDDDDDLLLUUUUUUURRDDDDDDLLUU";
bool swings(int index) { return (index >= 21 && index <= 29) || index == 36 || index == 37; }
constexpr int kNumbers[kTorObjects] = {7, 11, 19, 23, 27, 4, 9, 14, 17, 21, 8, 3};  // back numbers by object of the script
double facing_angle(char c) { return c == 'U' ? -kPi / 2 : c == 'D' ? kPi / 2 : c == 'L' ? kPi : 0; }

double wrap(double a) {
    while (a > kPi) a -= 2 * kPi;
    while (a <= -kPi) a += 2 * kPi;
    return a;
}
double angle_of(Vec v) { return std::atan2(v.y, v.x); }
Vec unit(double a) { return {std::cos(a), std::sin(a)}; }
double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// The original puts a 32 x 32 sprite with its top left corner at (x - 0x50, y - 0x60); the skates are at the bottom.
Vec ground(const TorObject& o) { return {o.x - kTorAnchorX + 16.0, o.y - kTorAnchorY + 26.0}; }
Vec small_centre(int x, int y) { return {x - kTorAnchorX + 2.5, y - kTorAnchorY + 2.5}; }

// Is the puck in the net of goal 0 (far) / 1 (near)?
bool in_net(Vec puck, int goal) {
    const double depth = goal ? puck.y - kGoalLineNear : kGoalLineFar - puck.y;
    return std::abs(puck.x - kRinkMidX) < kGoalHalfWidth - 1 && depth > 1.5 && depth < kGoalDepth + 8;
}

double base_scale(int width, int height, int bar) {
    return std::max(0.25, std::min(width / double(kTorViewWidth), (height - bar) / (kTorViewHeight + 2.0 * kSceneMargin)));
}

int tile_of(long long v) { return int(v >= 0 ? v / kTile : -((-v + kTile - 1) / kTile)); }

// Runs f(first row, end row) over the picture in horizontal bands, one thread each: the bands share no pixel.
template <class F>
void in_bands(int height, F f) {
    const int bands = std::clamp(int(std::thread::hardware_concurrency()), 1, std::max(1, std::min(16, height / 64)));
    std::vector<std::thread> workers;
    for (int b = 1; b < bands; b++) workers.emplace_back(f, height * b / bands, height * (b + 1) / bands);
    f(0, height / bands);
    for (std::thread& w : workers) w.join();
}

}  // namespace

Viewport scene_viewport(int width, int height, Vec camera, double zoom) {
    Viewport v;
    v.bar = int(height * 0.125 + 0.5);
    const double base = base_scale(width, height, v.bar);
    // closer only as far as the original's window still fits
    const double most = std::min(width / (kTorViewWidth * base), (height - v.bar) / (kTorViewHeight * base));
    const double z = std::clamp(zoom, 1.0, std::max(1.0, most));
    v.scale = base * z;
    const double vw = width / v.scale, vh = height / v.scale, above = (height - v.bar) / v.scale;
    v.origin = {camera.x + kTorViewWidth / 2.0 - vw / 2, camera.y + kTorViewHeight / 2.0 - above / 2};
    v.origin.x = vw >= kRinkWidth ? (kRinkWidth - vw) / 2 : std::clamp(v.origin.x, 0.0, kRinkWidth - vw);
    v.origin.y = vh >= kRinkHeight ? (kRinkHeight - vh) / 2 : std::clamp(v.origin.y, 0.0, kRinkHeight - vh);
    if (z == 1) {  // whole pixels: the arena is copied, not resampled
        v.origin = {std::round(v.origin.x * v.scale) / v.scale, std::round(v.origin.y * v.scale) / v.scale};
        if (vw < kRinkWidth) v.origin.x = std::min(v.origin.x, std::floor((kRinkWidth - vw) * v.scale) / v.scale);
        if (vh < kRinkHeight) v.origin.y = std::min(v.origin.y, std::floor((kRinkHeight - vh) * v.scale) / v.scale);
    }
    return v;
}

SceneTrack::SceneTrack(const TorScene& script, int attacking) {
    const int n_ticks = int(script.frames.size());
    const int goalie_team = attacking ? 0 : 1;
    std::vector<TorState> states;
    for (int n = 0; n <= n_ticks; n++) states.push_back(tor_state_at(script, n));
    moments_.resize(size_t(n_ticks) + 1);
    script_camera_.resize(size_t(n_ticks) + 1);
    for (int n = 0; n <= n_ticks; n++) {
        script_camera_[size_t(n)] = {double(states[size_t(n)].camera_x), double(states[size_t(n)].camera_y)};
        moments_[size_t(n)].puck = small_centre(states[size_t(n)].puck_x, states[size_t(n)].puck_y);
    }
    for (int n = 0; n <= n_ticks; n++) {
        const int a = std::max(0, n - 1), b = std::min(n_ticks, n + 1);
        moments_[size_t(n)].puck_velocity = b > a ? (moments_[size_t(b)].puck - moments_[size_t(a)].puck) * (1.0 / (b - a)) : Vec{};
        if (net_tick_ < 0)
            for (int g = 0; g < 2; g++)
                if (in_net(moments_[size_t(n)].puck, g)) net_tick_ = n, net_goal_ = g;
    }
    // Camera: the script moves it in steps of whole pixels; a weighted mean over the ticks around (the future is
    // known) follows without lag. The picture comes a little closer while the puck is near the goal or fast.
    for (int n = 0; n <= n_ticks; n++) {
        Vec sum;
        double weight = 0, zoom = 0, zoom_weight = 0;
        for (int k = -9; k <= 9; k++) {
            const int i = std::clamp(n + k, 0, n_ticks);
            const double w = std::exp(-k * k / 12.5), wz = std::exp(-k * k / 40.0);
            if (std::abs(k) <= 6) sum = sum + script_camera_[size_t(i)] * w, weight += w;
            const SceneMoment& m = moments_[size_t(i)];
            const double to_goal = std::min(length(m.puck - Vec{kRinkMidX, kGoalLineFar}), length(m.puck - Vec{kRinkMidX, kGoalLineNear}));
            zoom += wz * ((to_goal < 85 || length(m.puck_velocity) > 4.5) ? 1.07 : 1.0), zoom_weight += wz;
        }
        moments_[size_t(n)].camera = sum * (1 / weight);
        moments_[size_t(n)].zoom = zoom / zoom_weight;
    }
    for (size_t o = 0; o < kTorObjects; o++) {
        std::vector<Vec> pos(size_t(n_ticks) + 1), velocity(size_t(n_ticks) + 1);
        std::vector<TorSpriteRef> ref(size_t(n_ticks) + 1);
        std::vector<bool> present(size_t(n_ticks) + 1);
        for (int n = 0; n <= n_ticks; n++) {
            const TorObject& obj = states[size_t(n)].objects[o];
            ref[size_t(n)] = tor_sprite_ref(obj.sprite);
            const bool placed = obj.x != 0 || obj.y != 0;
            present[size_t(n)] = placed && ref[size_t(n)].kind == TorSpriteRef::Kind::Sheet;
            pos[size_t(n)] = ground(obj);
            SceneObject& so = moments_[size_t(n)].objects[o];
            if (placed && ref[size_t(n)].kind == TorSpriteRef::Kind::Dot) {
                so.kind = SceneObject::Kind::Dot;
                so.state.pos = small_centre(obj.x, obj.y);
            }
        }
        // movement per tick, calmed over five ticks (the scripts move in whole pixels)
        const auto step = [&](int a, int b) -> Vec {
            if (a < 0 || b > n_ticks || !present[size_t(a)] || !present[size_t(b)]) return {};
            const Vec d = pos[size_t(b)] - pos[size_t(a)];
            return length(d) > 10 ? Vec{} : d;
        };
        for (int n = 0; n <= n_ticks; n++) {
            Vec sum;
            int count = 0;
            for (int k = n - 2; k <= n + 2; k++)
                if (k >= 0 && k < n_ticks && present[size_t(k)] && present[size_t(k) + 1]) sum = sum + step(k, k + 1), count++;
            velocity[size_t(n)] = count ? sum * (1.0 / count) : Vec{};
        }
        double heading = 0, stride = o * 1.7;
        bool started = false;
        int last_spray = -100;
        for (int n = 0; n <= n_ticks; n++) {
            if (!present[size_t(n)]) {
                started = false;
                continue;
            }
            SceneObject& so = moments_[size_t(n)].objects[o];
            const TorSpriteRef r = ref[size_t(n)];
            const bool goalie = r.sheet == 2;
            so.kind = SceneObject::Kind::Skater;
            so.team = goalie ? goalie_team : r.sheet;
            PlayerState& s = so.state;
            s.pos = pos[size_t(n)];
            s.goalie = goalie;
            s.number = goalie ? (goalie_team ? 30 : 1) : kNumbers[o];
            const Vec puck = moments_[size_t(n)].puck;
            const double hint = goalie ? (r.index < 6 ? -kPi / 2 : kPi / 2) : facing_angle(kFacing[std::clamp(r.index, 0, 39)]);
            const Vec v = velocity[size_t(n)];
            const double speed = length(v);
            double target = hint;
            const Vec to_puck = puck - s.pos;
            if (goalie) {  // watches the puck, but stays square to the rink
                if (length(to_puck) > 4) target = hint + std::clamp(wrap(angle_of(to_puck) - hint), -1.0, 1.0);
            } else if (speed > 0.9) {  // skates forwards, or backwards when the sprite faced the other way
                target = angle_of(v);
                if (std::cos(target - hint) < -0.35) target = wrap(target + kPi);
            } else if (length(to_puck) > 6 && length(to_puck) < 170 && std::cos(angle_of(to_puck) - hint) > 0.2) {
                target = angle_of(to_puck);  // stands and looks at the puck
            }
            heading = started ? heading + std::clamp(wrap(target - heading) * 0.55, -0.5, 0.5) : target;
            started = true;
            stride += goalie ? 0 : speed * 0.40;
            s.heading = heading;
            s.stride = stride;
            s.effort = goalie ? 0 : std::clamp(speed / 3.2, 0.12, 1.0);
            s.has_puck = length(to_puck) < 26;
            s.puck = puck;
            // braking hard throws up ice
            if (!goalie && n >= 2 && n < n_ticks && n - last_spray > 8) {
                const double before = length(velocity[size_t(n) - 2]), after = length(velocity[size_t(n) + 1]);
                if (before > 2.6 && after < before * 0.45) sprays_.push_back({n, s.pos, normal(velocity[size_t(n) - 2])}), last_spray = n;
            }
        }
        // stick up: a shot when the puck is at the stick, otherwise joy
        for (int n = 0; n <= n_ticks;) {
            const auto act = [&](int k) { return present[size_t(k)] && ref[size_t(k)].sheet != 2 && swings(ref[size_t(k)].index); };
            if (!act(n)) {
                n++;
                continue;
            }
            int end = n;
            while (end <= n_ticks && act(end)) end++;
            bool near_puck = false;
            for (int k = n; k < end; k++) near_puck = near_puck || length(moments_[size_t(k)].puck - pos[size_t(k)]) < 34;
            for (int k = n; k < end; k++) {
                PlayerState& s = moments_[size_t(k)].objects[o].state;
                s.action = near_puck ? PlayerState::Action::Shoot : PlayerState::Action::Cheer;
                s.swing = (k - n + 0.5) / double(end - n);
            }
            n = end;
        }
        for (int n = 0; n <= n_ticks; n++) {  // goalie: goes down when a fast puck is close
            SceneObject& so = moments_[size_t(n)].objects[o];
            if (so.kind != SceneObject::Kind::Skater || !so.state.goalie) continue;
            const SceneMoment& m = moments_[size_t(n)];
            if (length(m.puck - so.state.pos) < 60 && length(m.puck_velocity) > 3) so.state.action = PlayerState::Action::Shoot;
        }
    }
    // Who shot: the attacker who swung last before the puck reached the net, or else the one at the puck when it
    // became fastest
    const int until = net_tick_ >= 0 ? net_tick_ : n_ticks;
    for (int n = until; n >= 0 && scorer_ < 0; n--)
        for (size_t o = 0; o < kTorObjects; o++) {
            const SceneObject& so = moments_[size_t(n)].objects[o];
            if (so.kind == SceneObject::Kind::Skater && !so.state.goalie && so.team == attacking && so.state.action == PlayerState::Action::Shoot) scorer_ = int(o);
        }
    if (scorer_ < 0) {
        int fastest = 0;
        for (int n = 0; n <= until; n++)
            if (length(moments_[size_t(n)].puck_velocity) > length(moments_[size_t(fastest)].puck_velocity)) fastest = n;
        const SceneMoment& m = moments_[size_t(std::max(0, fastest - 2))];
        double nearest = 1e9;
        for (size_t o = 0; o < kTorObjects; o++) {
            const SceneObject& so = m.objects[o];
            if (so.kind != SceneObject::Kind::Skater || so.state.goalie || so.team != attacking) continue;
            if (length(so.state.pos - m.puck) < nearest) nearest = length(so.state.pos - m.puck), scorer_ = int(o);
        }
    }
}

SceneMoment SceneTrack::at(double tick, bool smooth) const {
    if (moments_.empty()) return {};
    const int last = ticks();
    const int n = std::clamp(int(std::floor(tick)), 0, last);
    const double t = smooth && n < last ? std::clamp(tick - n, 0.0, 1.0) : 0;
    SceneMoment m = moments_[size_t(n)];
    if (!smooth) {
        m.camera = script_camera_[size_t(n)];
        m.zoom = 1;
    }
    if (t <= 0) return m;
    const SceneMoment& b = moments_[size_t(n) + 1];
    m.camera = lerp(m.camera, b.camera, t);
    m.zoom += (b.zoom - m.zoom) * t;
    if (length(b.puck - m.puck) < 40) m.puck = lerp(m.puck, b.puck, t);
    m.puck_velocity = lerp(m.puck_velocity, b.puck_velocity, t);
    for (size_t o = 0; o < kTorObjects; o++) {
        SceneObject& x = m.objects[o];
        const SceneObject& y = b.objects[o];
        if (x.kind != y.kind || x.kind == SceneObject::Kind::None || length(y.state.pos - x.state.pos) > 12) continue;
        x.state.pos = lerp(x.state.pos, y.state.pos, t);
        x.state.heading += wrap(y.state.heading - x.state.heading) * t;
        x.state.stride += (y.state.stride - x.state.stride) * t;
        x.state.effort += (y.state.effort - x.state.effort) * t;
        if (x.state.action == y.state.action) x.state.swing += (y.state.swing - x.state.swing) * t;
        x.state.puck = m.puck;
    }
    return m;
}

MatchScene::MatchScene(const std::filesystem::path& data) : data_(data) {}

const SceneTrack& MatchScene::track(const std::string& script) {
    auto it = tracks_.find(script);
    if (it == tracks_.end()) {
        const size_t dot = script.find('.');
        const bool away_attacks = dot != std::string::npos && dot + 1 < script.size() && script[dot + 1] == 'a';
        it = tracks_.emplace(script, SceneTrack(decode_tor(read_bytes(data_ / "TORE" / script)), away_attacks ? 1 : 0)).first;
    }
    return it->second;
}

int MatchScene::length(const std::string& script) { return track(script).ticks(); }

// Paints the tiles of the arena under the pixel rectangle (at tile scale) that are not there yet, several at a time.
void MatchScene::prepare(Tiles& tiles, bool with_crowd, long long x0, long long y0, int width, int height) {
    std::vector<std::pair<std::pair<int, int>, Surface*>> missing;
    for (int ty = tile_of(y0); ty <= tile_of(y0 + height - 1); ty++)
        for (int tx = tile_of(x0); tx <= tile_of(x0 + width - 1); tx++)
            if (!tiles.count({tx, ty})) missing.push_back({{tx, ty}, &tiles.emplace(std::pair{tx, ty}, Surface(kTile, kTile)).first->second});
    if (missing.empty()) return;
    const double scale = tile_scale_;
    const auto work = [&](size_t first, size_t step) {
        for (size_t i = first; i < missing.size(); i += step) {
            Painter p{*missing[i].second, scale, {missing[i].first.first * kTile / scale, missing[i].first.second * kTile / scale}};
            rink_->paint(p, with_crowd);
        }
    };
    const size_t threads = std::clamp<size_t>(std::thread::hardware_concurrency(), 1, std::min<size_t>(16, missing.size()));
    std::vector<std::thread> workers;
    for (size_t t = 1; t < threads; t++) workers.emplace_back(work, t, threads);
    work(0, threads);
    for (std::thread& w : workers) w.join();
}

// Rows from..to of `out` from the tiles; (x0, y0) is the tile-scale pixel at the top left of `out`.
void MatchScene::copy_tiles(const Tiles& tiles, Surface& out, long long x0, long long y0, int from, int to) {
    for (int ty = tile_of(y0 + from); ty <= tile_of(y0 + to - 1); ty++)
        for (int tx = tile_of(x0); tx <= tile_of(x0 + out.width - 1); tx++) {
            const auto it = tiles.find({tx, ty});
            if (it == tiles.end()) continue;
            const long long left = (long long)tx * kTile - x0, top = (long long)ty * kTile - y0;
            const int c0 = int(std::max(0LL, left)), c1 = int(std::min<long long>(out.width, left + kTile));
            const int r0 = int(std::max<long long>(from, top)), r1 = int(std::min<long long>(to, top + kTile));
            for (int y = r0; y < r1; y++)
                std::copy_n(it->second.pixels.begin() + ptrdiff_t((y - top) * kTile + (c0 - left)), c1 - c0,
                            out.pixels.begin() + ptrdiff_t(size_t(y) * size_t(out.width) + size_t(c0)));
        }
}

void MatchScene::bar(Surface& out, const Viewport& view, const MatchSceneInput& scene) {
    const double B = view.bar, W = out.width, top = out.height - B, cx = W / 2;
    Painter p{out, 1, {0, 0}};
    const Rgb white{244, 246, 250}, soft{168, 184, 208}, gold{255, 208, 60};
    const TeamColours teams[2] = {team_colours(scene.colour[0]), team_colours(scene.colour[1])};
    // score
    const double score_h = B * 0.36, score_y = top + B * 0.14;
    const bool goal = scene.final && scene.goal;
    p.box({cx, score_y + score_h / 2}, B * 0.62, score_h * 0.86, B * 0.1, 0, Rgb{255, 255, 255}, 0.09);
    draw_text(out, ":", cx, score_y, score_h, 1, soft, 1.0);
    for (int side = 0; side < 2; side++) {
        const bool scored = goal && scene.side == side;
        draw_text(out, std::to_string(scene.goals[size_t(side)]), cx + (side ? 1 : -1) * B * 0.32, score_y, score_h, 1, scored ? gold : white, 1.15);
    }
    // clubs with their colours
    const double room = cx - B * 0.95 - B * 0.4;
    for (int side = 0; side < 2; side++) {
        const std::string& name = scene.names[size_t(side)];
        double h = B * 0.23;
        if (text_width(name, h) > room) h *= room / text_width(name, h);
        const double dir = side ? 1 : -1, x = cx + dir * B * 0.95, y = top + B * 0.14 + (score_h - h) / 2;
        draw_text(out, name, x, y, h, side ? 0 : 2, white, 1.0);
        p.box({cx + dir * B * 0.74, score_y + score_h / 2}, B * 0.045, score_h * 0.86, B * 0.03, 0, teams[side].shirt, 1, B * 0.012, white);
    }
    // attendance and minute
    const double small = B * 0.15, small_y = top + B * 0.70;
    draw_text(out, scene.info, B * 0.4, small_y, small, 0, soft);
    draw_text(out, std::to_string(scene.minute) + ". MINUTE", W - B * 0.4, small_y, small, 2, soft);
    if (goal) draw_text(out, "TOR!", cx, small_y - small * 0.15, small * 1.3, 1, gold, 1.2);
}

void MatchScene::render(Surface& out, const MatchSceneInput& scene, double fraction, bool smooth) {
    static const SceneTrack empty;
    render(out, scene.script.empty() ? empty : track(scene.script), scene, fraction, smooth);
}

void MatchScene::render(Surface& out, const SceneTrack& tr, const MatchSceneInput& scene, double fraction, bool smooth) {
    if (out.width <= 0 || out.height <= 0) return;
    const int level = scene.arena >= 1 && scene.arena <= 4 ? scene.arena : 0;
    const TeamColours teams[2] = {team_colours(scene.colour[0]), team_colours(scene.colour[1])};
    if (!rink_ || level != rink_level_ || scene.colour != rink_colour_) {
        rink_ = std::make_unique<RinkArt>(level, teams[0].shirt, teams[1].shirt);
        rink_level_ = level, rink_colour_ = scene.colour;
        tiles_[0].clear(), tiles_[1].clear();
    }
    const bool playing = tr.ticks() >= 0;
    const double now = scene.frame + (smooth ? fraction : 0);
    const SceneMoment m = playing ? tr.at(scene.frame + fraction, smooth) : SceneMoment{};
    const Viewport view = scene_viewport(out.width, out.height, m.camera, m.zoom);
    const bool celebrate = playing && scene.final && scene.goal;

    // The arena lies in tiles at the scale of the unzoomed picture; a zoomed picture is resampled from them.
    const double base = base_scale(out.width, out.height, view.bar);
    if (tile_scale_ != base || tiles_[0].size() + tiles_[1].size() > 700) tiles_[0].clear(), tiles_[1].clear(), tile_scale_ = base;
    Tiles& tiles = tiles_[celebrate ? 0 : 1];
    const bool copied = view.scale == base;
    const long long x0 = copied ? std::llround(view.origin.x * base) : (long long)std::floor(view.origin.x * base) - 1;
    const long long y0 = copied ? std::llround(view.origin.y * base) : (long long)std::floor(view.origin.y * base) - 1;
    const int source_w = copied ? out.width : int(std::ceil(out.width / view.scale * base)) + 3;
    const int source_h = copied ? out.height : int(std::ceil(out.height / view.scale * base)) + 3;
    prepare(tiles, !celebrate, x0, y0, source_w, source_h);
    if (!copied) {
        if (scratch_.width != out.width + 3 || scratch_.height != out.height + 3) scratch_ = Surface(out.width + 3, out.height + 3);  // never smaller than the source
        in_bands(source_h, [&](int from, int to) { copy_tiles(tiles, scratch_, x0, y0, from, to); });
    }

    // Fresh traces of the skates behind every moving skater, fading over a second
    struct Trace {
        Vec a, b;
        double alpha;
    };
    std::vector<Trace> traces;
    if (playing) {
        constexpr int kSteps = 22;
        SceneMoment later = m;
        for (int k = 1; k <= kSteps; k++) {
            const SceneMoment earlier = tr.at(std::max(0.0, now - k * 0.8), true);
            for (size_t o = 0; o < kTorObjects; o++) {
                const SceneObject &x = later.objects[o], &y = earlier.objects[o];
                if (x.kind != SceneObject::Kind::Skater || y.kind != x.kind || x.state.goalie) continue;
                const Vec d = x.state.pos - y.state.pos;
                if (em4k::length(d) < 0.6 || em4k::length(d) > 14) continue;
                const Vec side = perp(normal(d)) * 3.4;
                const double alpha = 0.5 * (1 - (k - 1) / double(kSteps));
                traces.push_back({y.state.pos + side, x.state.pos + side, alpha});
                traces.push_back({y.state.pos - side, x.state.pos - side, alpha});
            }
            later = earlier;
        }
    }
    // The net gives way where the puck hits it and swings back
    const bool netted = playing && tr.net_tick() >= 0 && now >= tr.net_tick() && in_net(m.puck, tr.net_goal());
    const double since_net = netted ? (now - tr.net_tick()) * kTickSeconds + scene.after : 0;
    const double bulge = netted ? std::max(0.0, 1.6 + 5.5 * std::exp(-since_net * 4) * std::cos(since_net * 20)) : 0;
    const int lamp_goal = tr.net_tick() >= 0 ? tr.net_goal() : (m.camera.y + kTorViewHeight / 2.0 > kRinkMidY ? 1 : 0);
    // The scorer's name
    Vec label_at;
    bool label = false;
    if (celebrate && !scene.scorer.empty() && tr.scorer() >= 0 && m.objects[size_t(tr.scorer())].kind == SceneObject::Kind::Skater) {
        label_at = (m.objects[size_t(tr.scorer())].state.pos - Vec{0, 17} - view.origin) * view.scale;
        label = true;
    }

    std::vector<uint32_t> columns;  // per picture column: source column * 256 + weight of the next one
    if (!copied) {
        columns.resize(size_t(out.width));
        const double step = base / view.scale;
        for (int x = 0; x < out.width; x++) {
            const double u = (view.origin.x * base - x0) + (x + 0.5) * step - 0.5;
            const int ix = std::clamp(int(u), 0, scratch_.width - 2);
            columns[size_t(x)] = uint32_t(ix) << 8 | uint32_t(std::min(255.0, clamp01(u - ix) * 256));
        }
    }

    in_bands(out.height, [&](int top, int bottom) {
        if (copied) {
            copy_tiles(tiles, out, x0, y0, top, bottom);
        } else {  // bilinear from the assembled arena, in fixed point
            const double step = base / view.scale;
            const auto mix2 = [](uint32_t a, uint32_t b, uint32_t f) {  // per channel a + (b - a) * f / 256
                const uint32_t rb = ((a & 0xFF00FF) * (256 - f) + (b & 0xFF00FF) * f) >> 8 & 0xFF00FF;
                const uint32_t g = ((a & 0x00FF00) * (256 - f) + (b & 0x00FF00) * f) >> 8 & 0x00FF00;
                return rb | g;
            };
            for (int y = top; y < bottom; y++) {
                const double v = (view.origin.y * base - y0) + (y + 0.5) * step - 0.5;
                const int iy = std::clamp(int(v), 0, scratch_.height - 2);
                const uint32_t fy = uint32_t(clamp01(v - iy) * 256);
                const uint32_t* row0 = &scratch_.pixels[size_t(iy) * size_t(scratch_.width)];
                const uint32_t* row1 = row0 + scratch_.width;
                uint32_t* dst = &out.pixels[size_t(y) * size_t(out.width)];
                for (int x = 0; x < out.width; x++) {
                    const uint32_t ix = columns[size_t(x)] >> 8, fx = columns[size_t(x)] & 255;
                    dst[x] = 0xFF000000u | mix2(mix2(row0[ix], row0[ix + 1], fx), mix2(row1[ix], row1[ix + 1], fx), fy);
                }
            }
        }
        const auto panel = [&] {  // the score bar, see bar()
            Painter flat{out, 1, {0, 0}, top, bottom};
            const double y = out.height - view.bar;
            flat.rect(0, y, out.width, out.height, Rgb{12, 20, 38}, 0.84);
            flat.rect(0, y, out.width, y + std::max(1.0, view.bar * 0.025), Rgb{255, 255, 255}, 0.3);
        };
        Painter p{out, view.scale, view.origin, top, bottom};
        if (celebrate) rink_->paint_crowd(p, scene.after, scene.side == 0);
        for (int g = 0; g < 2; g++) RinkArt::paint_goal(p, g, m.puck.x, netted && tr.net_goal() == g ? bulge : 0);
        if (!playing) return panel();
        for (const Trace& t : traces) p.line(t.a, t.b, 0.5, Rgb{255, 255, 255}, t.alpha);
        for (const SceneSpray& s : tr.sprays()) {  // ice thrown up by a braking skater
            const double age = now - s.tick;
            if (age < 0 || age >= 10) continue;
            const double out_t = 1 - (1 - age / 10) * (1 - age / 10), base_angle = std::atan2(s.direction.y, s.direction.x);
            for (uint32_t i = 0; i < 18; i++) {
                const double a = base_angle + (hash01(i, uint32_t(s.tick), 1) - 0.5) * 1.7;
                const Vec at = s.pos + s.direction * 3 + Vec{std::cos(a), std::sin(a)} * ((3 + hash01(i, uint32_t(s.tick), 2) * 15) * out_t);
                p.circle(at, 0.5 + hash01(i, uint32_t(s.tick), 3) * 0.9, Rgb{255, 255, 255}, 0.9 * (1 - age / 10));
            }
        }
        if (celebrate) {  // the goal light behind the net
            const Vec at{kRinkMidX, lamp_goal ? 1062.0 : 56.0};
            const double pulse = 0.65 + 0.35 * std::cos(scene.after * 9);
            p.soft(at, 46, 30, Rgb{255, 40, 20}, 0.8 * pulse);
            p.circle(at, 3.4, Rgb{255, 230, 200}, 1, 0.8, Rgb{200, 20, 0});
        }
        for (int pass = 0; pass < 2; pass++)  // goalies below the skaters
            for (const SceneObject& o : m.objects)
                if (o.kind == SceneObject::Kind::Skater && o.state.goalie == (pass == 0)) draw_player(p, o.state, teams[o.team & 1]);
        // The script's puck is its place on the ice (the original draws it in the colour of the shadows); object type 0, the
        // grey dot, is the puck itself, apart from the shadow while it flies.
        Vec body = m.puck;
        double nearest = 90;
        for (const SceneObject& o : m.objects)
            if (o.kind == SceneObject::Kind::Dot && em4k::length(o.state.pos - m.puck) < nearest) nearest = em4k::length(o.state.pos - m.puck), body = o.state.pos;
        draw_puck(p, m.puck, body, m.puck_velocity);
        if (label) {
            Painter flat{out, 1, {0, 0}, top, bottom};
            const double cap = std::max(9.0, out.height * 0.02), pop = clamp01(scene.after / 0.18), width = text_width(scene.scorer, cap);
            const Vec c{label_at.x, label_at.y - cap * (0.4 + 0.6 * pop)};
            flat.box(c, width / 2 + cap * 0.9, cap * 0.95, cap * 0.5, 0, Rgb{12, 20, 38}, 0.88 * pop, cap * 0.1, teams[scene.side & 1].shirt);
            draw_text(flat, scene.scorer, {c.x, c.y - cap / 2}, {1, 0}, {0, 1}, cap, 1, Rgb{255, 255, 255}, 1.05, pop);
        }
        panel();
    });
    bar(out, view, scene);
}

}  // namespace em4k
