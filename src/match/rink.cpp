// SPDX-License-Identifier: GPL-3.0-only
#include "match/rink.hpp"

#include <array>

#include "match/text.hpp"

namespace em4k {
namespace {

// Layout measured in the rink picture of the original (map KB236): the ice is x 97..574, y 83..1068 behind the near board that
// hides it from y 1057; goal lines y 128.5 / 1023, blue lines y 385.75 / 765.75, centre line y 575.75; the boards
// reach x 80..593 and y 39 (top of the far glass) .. 1070.
constexpr Vec kOuterC{336.5, 554.5}, kOuterH{256.5, 515.5};
constexpr double kOuterR = 58;
constexpr Vec kIceC{335.75, 570}, kIceH{239.25, 487};
constexpr double kIceR = 44;
constexpr double kMidY = kRinkMidY, kMidX = kRinkMidX;

constexpr Rgb kInk{32, 48, 65};        // outline colour of the original (index C8)
constexpr Rgb kLine{210, 44, 40};      // goal lines, centre line and face-off circles
constexpr Rgb kBlue{0, 97, 227};
constexpr Rgb kRed{243, 32, 0};
constexpr Rgb kIce{142, 227, 227};
constexpr Rgb kWhite{255, 255, 255};

constexpr int kGridCell = 64, kGridW = 11, kGridH = 18;

double outer(Vec p) { return sd_box(p, kOuterC, kOuterH.x, kOuterH.y, kOuterR); }
double ice(Vec p) { return sd_box(p, kIceC, kIceH.x, kIceH.y, kIceR); }

double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// 0 = left stand, 1 = right, 2 = far (top), 3 = near (bottom), +4 = corner section of that side pair
int stand_of(Vec p) {
    const double mx = p.x < 336 ? p.x : 672 - p.x;
    const int side = p.x < 336 ? 0 : 1;
    if (p.y < 576) {
        if (p.y > 86 + 0.25 * mx) return side;
        if (mx > 75 + 1.667 * p.y) return 2;
        return 4 + side;
    }
    if (p.y < 1024 + 0.13 * mx) return side;
    if (mx > 150 - (p.y - 1072) * 0.8125) return 3;
    return 6 + side;
}
Vec facing(int stand) {
    constexpr double d = 0.70710678;
    switch (stand) {
        case 0: return {1, 0};
        case 1: return {-1, 0};
        case 2: return {0, 1};
        case 3: return {0, -1};
        case 4: return {d, d};
        case 5: return {-d, d};
        case 6: return {d, -d};
        default: return {-d, -d};
    }
}

// Advertising on the boards: invented names
struct Ad {
    const char* name;
    Rgb back, ink;
};
constexpr int kAdCount = 8;
constexpr double kAdLeft = 126, kAdWidth = 52.5;
constexpr Ad kAds[kAdCount] = {{"OPENEM", {22, 44, 110}, {255, 255, 255}},     {"NORDBRÄU", {250, 200, 40}, {40, 30, 20}},
                               {"ALPENMILCH", {240, 244, 248}, {0, 110, 200}}, {"RADIO 7", {214, 40, 44}, {255, 255, 255}},
                               {"TURBO SPORT", {28, 30, 40}, {255, 208, 60}},  {"EISBÄR COLA", {0, 140, 200}, {255, 255, 255}},
                               {"BLITZ REISEN", {250, 140, 30}, {30, 30, 40}}, {"PUCK & CO", {30, 130, 80}, {255, 255, 255}}};

// The stands get darker away from the ice.
double depth_shade(Vec p) { return 1 - 0.36 * clamp01((outer(p) - 8) / 105); }

// Concrete of the stands: a walkway around the boards, the dividers between the sections (they lie on the borders of
// stand_of) and the stairs of the far and near stand. Negative inside.
double concrete_sd(Vec p, bool* stairs) {
    const double mx = p.x < 336 ? p.x : 672 - p.x;
    const Vec q{mx, p.y};
    double d = outer(p) - 6;
    d = std::min(d, sd_capsule(q, {-10, 83.5}, {90, 108.5}, 6, 6));
    d = std::min(d, sd_capsule(q, {68, -4}, {150, 45}, 6, 6));
    d = std::min(d, sd_capsule(q, {-10, 1022.7}, {96, 1036.5}, 6, 6));
    d = std::min(d, sd_capsule(q, {156, 1065}, {80, 1158}, 6, 6));
    for (double y : {490.0, 662.0}) d = std::min(d, sd_box(p, {67, y}, 11, 64, 2));        // benches
    for (double y : {524.0, 628.0}) d = std::min(d, sd_box(p, {606, y}, 11, 36, 2));       // penalty boxes
    d = std::min(d, sd_box(p, {606, 575.75}, 11, 17, 2));                                  // scorers
    double st = 1e9;
    for (double x : {247.0, 392.0}) {
        st = std::min(st, sd_box(p, {x, 14}, 4.5, 34, 0));
        st = std::min(st, sd_box(p, {x, 1116}, 4.5, 44, 0));
    }
    if (stairs) *stairs = st < d;
    return std::min(d, st);
}

// Rows of seats, 8 x 8 units each as in the original, the back rest away from the ice
Rgb seat_colour(Vec p, double scale) {
    const Vec f = facing(stand_of(p));
    const double u = p.x * -f.y + p.y * f.x, v = p.x * f.x + p.y * f.y;
    const Vec q{u - std::floor(u / 8) * 8, v - std::floor(v / 8) * 8};
    Rgb c{96, 8, 8};
    const double cushion = clamp01(0.5 - sd_box(q, {4, 5.1}, 3.0, 1.7, 0.8) * scale);
    c = mix(c, Rgb{196, 16, 6}, cushion);
    const double back = clamp01(0.5 - sd_box(q, {4, 2.1}, 3.3, 1.3, 0.8) * scale);
    c = mix(c, Rgb{243, 40, 8}, back);
    const double shine = clamp01(0.5 - sd_box(q, {4, 1.5}, 2.5, 0.35, 0.3) * scale);
    return mix(c, Rgb{255, 120, 70}, shine * 0.8);
}

double value_noise(Vec p, double period, uint32_t seed) {
    const double x = p.x / period, y = p.y / period;
    const double fx = std::floor(x), fy = std::floor(y);
    const double tx = x - fx, ty = y - fy;
    const double sx = tx * tx * (3 - 2 * tx), sy = ty * ty * (3 - 2 * ty);
    const auto h = [&](double ix, double iy) { return hash01(uint32_t(int(ix) + 4096), uint32_t(int(iy) + 4096), seed); };
    const double a = h(fx, fy) + (h(fx + 1, fy) - h(fx, fy)) * sx, b = h(fx, fy + 1) + (h(fx + 1, fy + 1) - h(fx, fy + 1)) * sx;
    return a + (b - a) * sy;
}

}  // namespace

namespace {
constexpr int kGridCells = kGridW * kGridH;
int cell_of(double v, int n) { return std::clamp(int(std::floor(v / kGridCell)), 0, n - 1); }
}  // namespace

RinkArt::RinkArt(int level, Rgb home, Rgb away) : home_(home), away_(away) {
    build_crowd(std::clamp(level, 0, 4));
    build_ice();
    ice_grid_.assign(kGridCells, {});
    for (size_t i = 0; i < ice_.size(); i++) {
        const Item& it = ice_[i];
        const double r = (it.kind == Item::Capsule ? std::max(it.ra, it.rb) : it.kind == Item::Box ? length(it.b) : it.kind == Item::Ring ? it.ra + it.rb : it.ra) + it.line + 1;
        const Vec lo = it.kind == Item::Capsule ? Vec{std::min(it.a.x, it.b.x), std::min(it.a.y, it.b.y)} : it.a;
        const Vec hi = it.kind == Item::Capsule ? Vec{std::max(it.a.x, it.b.x), std::max(it.a.y, it.b.y)} : it.a;
        for (int cy = cell_of(lo.y - r, kGridH); cy <= cell_of(hi.y + r, kGridH); cy++)
            for (int cx = cell_of(lo.x - r, kGridW); cx <= cell_of(hi.x + r, kGridW); cx++) ice_grid_[size_t(cy) * kGridW + size_t(cx)].push_back(uint32_t(i));
    }
    crowd_grid_.assign(kGridCells, {});
    for (size_t i = 0; i < crowd_.size(); i++)
        crowd_grid_[size_t(cell_of(crowd_[i].seat.y, kGridH)) * kGridW + size_t(cell_of(crowd_[i].seat.x, kGridW))].push_back(uint32_t(i));
}

void RinkArt::paint_ice_items(Painter& p) const {
    const double x0 = p.origin.x, y0 = p.origin.y, x1 = x0 + p.target.width / p.scale, y1 = y0 + p.target.height / p.scale;
    if (x1 < 0 || y1 < 0 || x0 > kRinkWidth || y0 > kRinkHeight) return;
    std::vector<uint32_t> ids;
    for (int cy = cell_of(y0, kGridH); cy <= cell_of(y1, kGridH); cy++)
        for (int cx = cell_of(x0, kGridW); cx <= cell_of(x1, kGridW); cx++) {
            const auto& cell = ice_grid_[size_t(cy) * kGridW + size_t(cx)];
            ids.insert(ids.end(), cell.begin(), cell.end());
        }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    for (uint32_t id : ids) {
        const Item& it = ice_[id];
        switch (it.kind) {
            case Item::Circle: p.circle(it.a, it.ra, it.fill, it.alpha, it.line, it.line_colour); break;
            case Item::Capsule: p.capsule(it.a, it.b, it.ra, it.rb, it.fill, it.alpha, it.line, it.line_colour); break;
            case Item::Box: p.box(it.a, it.b.x, it.b.y, it.ra, it.rb, it.fill, it.alpha, it.line, it.line_colour); break;
            case Item::Ring: p.ring(it.a, it.ra, it.rb, it.fill, it.alpha); break;
        }
    }
}

// Calls f for every spectator near the part of the arena the painter shows.
template <class F>
void RinkArt::each_spectator(const Painter& p, F f) const {
    const double x0 = p.origin.x - 8, x1 = p.origin.x + p.target.width / p.scale + 8;
    const double y0 = p.origin.y + p.clip_top / p.scale - 8, y1 = p.origin.y + std::min(p.target.height, p.clip_bottom) / p.scale + 8;
    if (x1 < 0 || y1 < 0 || x0 > kRinkWidth || y0 > kRinkHeight) return;
    for (int cy = cell_of(y0, kGridH); cy <= cell_of(y1, kGridH); cy++)
        for (int cx = cell_of(x0, kGridW); cx <= cell_of(x1, kGridW); cx++)
            for (uint32_t id : crowd_grid_[size_t(cy) * kGridW + size_t(cx)]) {
                const Spectator& s = crowd_[id];
                if (s.seat.x > x0 && s.seat.x < x1 && s.seat.y > y0 && s.seat.y < y1) f(s);
            }
}

// One spectator seen from above, looking towards the ice. jump 0..1 lifts the figure towards the viewer.
void RinkArt::draw_spectator(Painter& target, const Spectator& s, double jump) const {
    static constexpr Rgb skins[] = {{246, 204, 160}, {236, 184, 134}, {214, 154, 104}, {150, 102, 68}};
    static constexpr Rgb hairs[] = {{74, 46, 24}, {34, 30, 36}, {236, 190, 70}, {150, 76, 30}, {196, 196, 202}, {112, 72, 38}};
    static constexpr Rgb shirts[] = {{232, 236, 240}, {40, 60, 120}, {0, 110, 220}, {30, 150, 84}, {250, 200, 50}, {120, 130, 146},
                                     {44, 44, 56},    {134, 84, 52}, {232, 124, 164}, {250, 144, 44}, {150, 200, 236}, {96, 60, 130}};
    const Rgb ink{30, 22, 34};
    const Vec c = s.seat, f = s.facing, side{-f.y, f.x};
    const uint32_t seed = s.seed;
    Painter p = target;
    if (jump > 0) {  // nearer to the viewer: larger, and a little towards the ice
        const double k = 1 + 0.26 * jump;
        p.scale = target.scale * k;
        p.origin = c + (target.origin - c) * (1 / k) - f * (0.8 * jump);
    }
    const double angle = std::atan2(side.y, side.x);
    const double dim = depth_shade(c) + 0.12 * jump;
    const Rgb shirt = shade(s.fan ? home_ : shirts[hash32(seed, 2) % 12], dim);
    const Rgb skin = shade(skins[hash32(seed, 3) % 4], dim);
    Rgb hair = shade(hairs[hash32(seed, 4) % 6], dim);
    const uint32_t extra = hash32(seed, 5) % 28;
    const Vec body = c - f * 0.7, head = c + f * 0.5;
    if (extra % 7 == 0 || jump > 0.05)  // arms up
        for (int k = -1; k <= 1; k += 2) {
            const Vec hand = body + side * (k * 3.6) + f * 3.4;
            p.capsule(body + side * (k * 2.6), hand, 0.9, 0.8, shirt, 1, 0.3, ink);
            p.circle(hand, 0.85, skin, 1, 0.3, ink);
        }
    p.box(body, 3.2, 1.95, 1.8, angle, shirt, 1, 0.38, ink);
    if (s.fan) p.capsule(body - side * 2.0 + f * 0.9, body + side * 2.0 + f * 0.9, 0.5, 0.5, Rgb{244, 246, 250}, 0.95);
    else p.capsule(body - side * 1.9 - f * 0.5, body + side * 1.9 - f * 0.5, 0.7, 0.7, shade(shirt, 1.25), 0.6);
    if (extra == 1 || extra == 2) hair = skin;  // bald
    p.circle(head, 1.95, hair, 1, 0.38, ink);
    p.shape(head.x - 2, head.y - 2, head.x + 2, head.y + 2, [=](Vec q) { return std::max(sd_circle(q, head, 1.8), 0.75 - dot(q - head, f)); }, skin);
    if (extra == 3 || extra == 4 || extra == 5) {  // cap with a peak
        const Rgb cap = shade(extra != 5 ? home_ : shirts[hash32(seed, 6) % 12], dim);
        p.capsule(head + f * 0.9, head + f * 2.3, 1.25, 1.0, shade(cap, 0.8), 1, 0.3, ink);
        p.circle(head, 1.95, cap, 1, 0.38, ink);
        p.circle(head, 0.45, shade(cap, 1.4));
    }
}

void RinkArt::build_crowd(int level) {
    const double fill = 0.12 + 0.2 * level;
    for (int stand = 0; stand < 8; stand++) {
        const Vec f = facing(stand), side{-f.y, f.x};
        for (int cv = -150; cv <= 150; cv++)
            for (int cu = -150; cu <= 150; cu++) {
                const Vec c = side * (8.0 * cu + 4) + f * (8.0 * cv + 4.6);
                if (c.x < 3 || c.y < 3 || c.x > kRinkWidth - 3 || c.y > kRinkHeight - 3 || stand_of(c) != stand) continue;
                const double away = outer(c);
                if (away < 9 || concrete_sd(c, nullptr) < 3.2) continue;
                const double n = value_noise(c, 70, 21) * 0.6 + value_noise(c, 23, 22) * 0.4;
                const double chance = fill + (n - 0.5) * 0.8 + (0.5 - clamp01(away / 90)) * 0.3;
                const uint32_t seed = hash32(uint32_t(cu + 512), uint32_t(cv + 512), uint32_t(stand + 31));
                if (hash01(seed, 9) >= chance) continue;
                crowd_.push_back({c + side * (hash01(seed, 10) - 0.5) * 1.0, f, seed, hash32(seed, 1) % 100 < 42});
            }
    }
}

void RinkArt::paint_crowd(Painter& p, double time, bool home_scored) const {
    const double ramp = clamp01(time / 0.25);
    each_spectator(p, [&](const Spectator& s) {
        const double mood = hash01(s.seed, 40);
        const bool happy = s.fan ? home_scored : (home_scored ? mood < 0.45 : mood < 0.12);  // most neutral ones side with the home club
        const double jump = happy ? ramp * std::max(0.0, std::sin(time * (8 + mood * 4) + hash01(s.seed, 41) * 6.28)) : 0;
        draw_spectator(p, s, jump);
    });
}

// Benches of the two teams at the left board, penalty boxes and the scorers at the right one
void RinkArt::paint_benches(Painter& p) const {
    const Rgb ink{30, 22, 34}, wood{150, 104, 62};
    for (int k = 0; k < 2; k++) {
        const double y0 = k ? 606 : 434;
        const Rgb shirt = k ? home_ : away_;
        const Rgb helmet = 0.3 * shirt.r + 0.59 * shirt.g + 0.11 * shirt.b > 150 ? Rgb{26, 40, 84} : Rgb{240, 243, 248};
        p.capsule({61.5, y0}, {61.5, y0 + 112}, 1.6, 1.6, wood, 1, 0.4, ink);
        for (int i = 0; i < 7; i++) {
            const Vec c{66.5 + (hash01(uint32_t(i), uint32_t(k), 50) - 0.5) * 1.6, y0 + 8 + i * 16.0};
            const Vec head = c + Vec{1.6, 0};
            p.box(c - Vec{1.7, 0}, 3.3, 5.6, 2.8, 0, shirt, 1, 0.45, ink);
            p.line(c + Vec{-3.2, -3.6}, c + Vec{-3.2, 3.6}, 0.8, helmet, 0.8);
            p.circle(head, 2.9, helmet, 1, 0.45, ink);
            p.shape(head.x - 3, head.y - 3, head.x + 3, head.y + 3, [=](Vec q) { return std::max(sd_circle(q, head, 2.5), head.x + 1.3 - q.x); }, Rgb{44, 62, 96});
        }
    }
    for (int k = 0; k < 2; k++) p.capsule({610.5, k ? 600.0 : 496.0}, {610.5, k ? 656.0 : 552.0}, 1.6, 1.6, wood, 1, 0.4, ink);
    p.box({607, kMidY}, 5.5, 13, 1.5, 0, Rgb{214, 222, 232}, 1, 0.45, ink);
    for (int k = -1; k <= 1; k += 2) {
        const Vec c{613.5, kMidY + k * 6.5};
        p.box(c, 2.4, 4.4, 2.2, 0, Rgb{40, 44, 58}, 1, 0.4, ink);
        p.circle(c - Vec{1.2, 0}, 2.3, Rgb{112, 72, 38}, 1, 0.4, ink);
    }
}

void RinkArt::build_ice() {
    const auto line = [&](Vec a, Vec b, double w, Rgb c, float alpha = 1) {
        ice_.push_back({Item::Capsule, a, b, float(w / 2), float(w / 2), c, {}, alpha, 0});
    };
    // Traces of the skates and glints, denser around the face-off circles as in the original
    const auto inside = [](Vec p) { return ice(p) < -4 && p.y < 1052; };
    for (uint32_t i = 0; i < 2600; i++) {
        Vec p{97 + hash01(i, 1) * 478, 83 + hash01(i, 2) * 974};
        if (i % 3 == 0) {  // pulled towards one of the five circles
            const int k = int(hash32(i, 3) % 5);
            const Vec centre = k == 4 ? Vec{kMidX, kMidY} : Vec{k & 1 ? 449.0 : 225.0, k & 2 ? 925.5 : 226.0};
            const double a = hash01(i, 4) * 6.2832, r = hash01(i, 5) * 74;
            p = centre + Vec{std::cos(a), std::sin(a)} * r;
        }
        if (!inside(p)) continue;
        const double a = hash01(i, 6) * 6.2832, len = 1.5 + hash01(i, 7) * hash01(i, 8) * 10;
        const Vec d = Vec{std::cos(a), std::sin(a)} * len;
        if (!inside(p + d)) continue;
        if (i % 9 == 0) {  // glint: a small star
            const double s = 1.2 + hash01(i, 9) * 1.6;
            line(p - Vec{s, 0}, p + Vec{s, 0}, 0.35, kWhite, 0.85f);
            line(p - Vec{0, s}, p + Vec{0, s}, 0.35, kWhite, 0.85f);
            ice_.push_back({Item::Circle, p, {}, 0.55f, 0, kWhite, {}, 0.95f, 0});
        } else {
            const Vec bend = perp(d) * ((hash01(i, 10) - 0.5) * 0.5);
            line(p, p + d * 0.5 + bend, 0.3, kWhite, 0.5f);
            line(p + d * 0.5 + bend, p + d, 0.3, kWhite, 0.5f);
        }
    }
    // Lines
    line({97, 128.5}, {574.5, 128.5}, 1.7, kLine);
    line({97, 1023}, {574.5, 1023}, 1.7, kLine);
    for (double y : {385.75, 765.75}) line({97, y}, {574.5, y}, 2.4, kBlue);
    line({97, kMidY}, {574.5, kMidY}, 3.6, kLine);
    for (double x = 101; x < 572; x += 16) line({x, kMidY}, {x + 7, kMidY}, 1.0, Rgb{255, 226, 220}, 0.9f);
    ice_.push_back({Item::Ring, {kMidX, kMidY}, {}, 69, 2.0f, kBlue, {}, 1, 0});
    ice_.push_back({Item::Circle, {kMidX, kMidY}, {}, 6, 0, kBlue, {}, 1, 0});
    for (int k = 0; k < 4; k++) {
        const Vec c{k & 1 ? 449.0 : 225.0, k & 2 ? 925.5 : 226.0};
        ice_.push_back({Item::Ring, c, {}, 68.5f, 1.7f, kLine, {}, 1, 0});
        for (int s = -1; s <= 1; s += 2) {
            line({c.x - 14, c.y + s * 33.5}, {c.x + 14, c.y + s * 33.5}, 1.7, kLine);
            line({c.x + s * 69.5, c.y}, {c.x + s * 77.5, c.y}, 1.7, kLine);
        }
        ice_.push_back({Item::Circle, c, {}, 3.4f, 0, kRed, kLine, 1, 0.9f});
        const Vec dot{k & 1 ? 448.5 : 224.5, k & 2 ? 727.5 : 424.0};
        ice_.push_back({Item::Circle, dot, {}, 3.2f, 0, kRed, kLine, 1, 0.9f});
    }
    // Referee's crease at the right board: half of a ring, drawn as short strokes
    for (int i = 0; i < 48; i++) {
        const double a0 = 1.5708 + 3.14159 * i / 48, a1 = 1.5708 + 3.14159 * (i + 1) / 48;
        line(Vec{574.5, kMidY} + Vec{std::cos(a0), std::sin(a0)} * 46, Vec{574.5, kMidY} + Vec{std::cos(a1), std::sin(a1)} * 46, 1.7, kLine);
    }
}

namespace {

constexpr double kGoalHalf = kGoalHalfWidth, kCrease = 33;

// The net seen from above: the mouth on the goal line, rounded at the back. `out` is -1 / 1: where the net lies.
// A puck in the net pushes the back out by `bulge` around x = bulge_x.
double net_sd(Vec q, double gy, double out, double bulge_x, double bulge) {
    const double depth = (q.y - gy) * out;
    if (bulge != 0) {
        const double k = (q.x - bulge_x) / 9;
        q.y -= out * bulge * std::exp(-k * k) * clamp01(depth / kGoalDepth);
    }
    return std::max(sd_box(q, {kMidX, gy + out * (kGoalDepth - 14) / 2}, kGoalHalf, (kGoalDepth + 14) / 2, 12), -depth);
}

void paint_crease(Painter& p, double gy, double out) {
    const Vec c{kMidX, gy};
    const auto sd = [=](Vec q) { return std::max(sd_circle(q, c, kCrease), (q.y - gy) * out); };
    p.shape(c.x - kCrease, c.y - kCrease, c.x + kCrease, c.y + kCrease, sd, Rgb{96, 176, 240}, 0.5);
    p.shape(c.x - kCrease, c.y - kCrease, c.x + kCrease, c.y + kCrease, [=](Vec q) { return std::max(std::abs(sd_circle(q, c, kCrease)) - 0.8, (q.y - gy) * out); },
            kRed, 0.95);
}

}  // namespace

void RinkArt::paint_goal(Painter& p, int goal, double bx, double bulge) {
    const double gy = goal ? kGoalLineNear : kGoalLineFar, out = goal ? 1 : -1;
    const double reach = kGoalDepth + std::max(0.0, bulge);
    const double x0 = kMidX - kGoalHalf - 4, x1 = kMidX + kGoalHalf + 6;
    const double y0 = std::min(gy, gy + out * reach) - 4, y1 = std::max(gy, gy + out * reach) + 6;
    const double scale = p.scale;
    const auto net = [=](Vec q) { return net_sd(q, gy, out, bx, bulge); };
    p.shape(x0, y0, x1, y1, [=](Vec q) { return net(q - Vec{2.6, 3.4}) - 1; }, Rgb{16, 70, 120}, 0.28);  // shadow
    p.shape(x0, y0, x1, y1, net, Rgb{240, 246, 250}, 0.6);
    p.cover(x0, y0, x1, y1, Rgb{96, 122, 150}, [=](Vec q) {  // mesh, stretched where the net bulges
        const double inside = clamp01(0.5 - net(q) * scale);
        if (inside <= 0) return 0.0;
        const double k = (q.x - bx) / 9, pull = bulge * std::exp(-k * k) * clamp01((q.y - gy) * out / kGoalDepth);
        const double y = q.y - out * pull;
        const double u = (q.x + y) / 3.4, v = (q.x - y) / 3.4;
        const double d = std::min(std::abs(u - std::round(u)), std::abs(v - std::round(v))) * 3.4 * 0.7071;
        return inside * clamp01(0.5 - (d - 0.22) * scale) * 0.85;
    });
    // the top of the net: a lighter panel behind the crossbar
    p.shape(x0, y0, x1, y1, [=](Vec q) { return std::max(std::abs(net(q) + 6.5) - 0.45, 0.4 - (q.y - gy) * out); }, Rgb{255, 255, 255}, 0.75);
    // frame: base of the net and the crossbar over the goal line
    p.shape(x0, y0, x1, y1, [=](Vec q) { return std::abs(net(q)) - 1.75; }, Rgb{120, 10, 0});
    p.shape(x0, y0, x1, y1, [=](Vec q) { return std::abs(net(q)) - 1.15; }, kRed);
    p.shape(x0, y0, x1, y1, [=](Vec q) { return std::abs(net(q + Vec{0.35, 0.4})) - 0.3; }, Rgb{255, 150, 120}, 0.8);
    for (int k = -1; k <= 1; k += 2) {
        const Vec post{kMidX + k * kGoalHalf, gy};
        p.circle(post, 2.3, kRed, 1, 0.6, Rgb{120, 10, 0});
        p.circle(post - Vec{0.6, 0.7}, 0.8, Rgb{255, 170, 140}, 0.9);
    }
}

void RinkArt::paint(Painter& p, bool with_crowd) const {
    Surface& s = p.target;
    const double scale = p.scale;
    const Rgb dark{24, 14, 18};
    // Stands
    for (int y = 0; y < s.height; y++)
        for (int x = 0; x < s.width; x++) {
            const Vec w{p.origin.x + (x + 0.5) / scale, p.origin.y + (y + 0.5) / scale};
            Rgb c = dark;
            if (w.x >= 0 && w.y >= 0 && w.x < kRinkWidth && w.y < kRinkHeight) {
                if (outer(w) * scale < -1.5) continue;  // boards and ice follow below
                c = shade(seat_colour(w, scale), depth_shade(w));
                bool stairs = false;
                const double d = -concrete_sd(w, &stairs);  // world units inside the concrete
                if (d * scale > -1.5 - 0.8 * scale) {
                    const double n = value_noise(w, 9, 5) * 0.10 + value_noise(w, 2.2, 6) * 0.05;
                    Rgb top = shade(Rgb{112, 128, 146}, 0.92 + n + clamp01(d / 6) * 0.22);
                    if (stairs) {  // steps
                        const double step = w.y / 3.4 - std::floor(w.y / 3.4);
                        top = shade(Rgb{126, 142, 160}, step < 0.28 ? 0.62 : 0.95 + step * 0.25);
                    }
                    c = mix(c, kInk, clamp01(0.5 + (d + 0.8) * scale));
                    c = mix(c, top, clamp01(0.5 + d * scale));
                }
            }
            s.pixels[size_t(y) * size_t(s.width) + size_t(x)] = Surface::pack(c);
        }
    paint_benches(p);
    if (with_crowd) each_spectator(p, [&](const Spectator& sp) { draw_spectator(p, sp, 0); });
    // Boards, glass and ice
    for (int y = 0; y < s.height; y++)
        for (int x = 0; x < s.width; x++) {
            const Vec w{p.origin.x + (x + 0.5) / scale, p.origin.y + (y + 0.5) / scale};
            const double dO = outer(w);
            const double cover = 0.5 - dO * scale;
            if (cover <= 0) continue;
            const double dI = ice(w);
            const bool near_board = w.y > 1057 && dI < 0;  // the near board stands in front of the ice
            double alpha = 1;
            Rgb c;
            // the ice
            const double n = value_noise(w, 46, 1) * 0.6 + value_noise(w, 13, 2) * 0.4;
            Rgb icec = mix(kIce, Rgb{176, 238, 238}, n * 0.55);
            icec = mix(icec, Rgb{120, 214, 222}, clamp01((dI + 7) / 7) * 0.5);  // darker along the boards
            {  // the lights of the hall mirrored in the ice
                const double lx = (w.x - 16 - std::round((w.x - 16) / 160) * 160) / 54, ly = (w.y - 76 - std::round((w.y - 76) / 150) * 150) / 21;
                const double glow = std::max(0.0, 1 - lx * lx - ly * ly);
                icec = mix(icec, Rgb{236, 252, 252}, glow * glow * glow * 0.45);
            }
            if (dI * scale < -0.5 && !near_board) {
                c = icec;
            } else {
                // the band: kick plate, board, glass with posts and glints, top rail
                const double t = near_board ? w.y - 1057 : dI;
                // how much the wall faces the viewer: the far wall shows its whole inner side, the side walls little
                const double up = near_board ? 0 : clamp01(-(ice({w.x, w.y + 0.5}) - ice({w.x, w.y - 0.5})));
                const double wall = 5.5 + 9.5 * up * up * (3 - 2 * up);
                const bool far_wall = up > 0.5, side_wall = !far_wall && !near_board;
                Rgb band;
                if (t < 1.6) band = Rgb{69, 178, 190};
                else if (t < wall) {
                    band = mix(Rgb{236, 240, 244}, Rgb{186, 200, 214}, clamp01(t / wall));
                    if (t > wall * 0.78 && t < wall * 0.88) band = Rgb{255, 199, 0};
                    else if (far_wall && t > 2.4 && t < wall * 0.74 && w.x > kAdLeft && w.x < kAdLeft + kAdWidth * kAdCount) {
                        const double k = (w.x - kAdLeft) / kAdWidth, edge = std::abs(k - std::round(k)) * kAdWidth;
                        band = edge < 0.5 ? Rgb{236, 240, 244} : kAds[std::clamp(int(k), 0, kAdCount - 1)].back;
                    } else if (!far_wall && up < 0.2 && t > 2.3 && t < 4.7) {  // colour fields on the side boards
                        const double k = w.y / 44;
                        if (std::abs(k - std::round(k)) * 44 > 0.5) band = kAds[size_t(hash32(uint32_t(k), w.x < 336 ? 1 : 2) % kAdCount)].back;
                    }
                } else {
                    const double along = far_wall || near_board ? w.x : w.y;
                    const double post = std::abs(along - std::round(along / 32) * 32);
                    const double streak = (w.x + w.y * (side_wall ? 0.35 : 1.0)) / 9.0;
                    const double cell = std::floor(streak);
                    const double f = streak - cell;
                    band = mix(Rgb{116, 142, 164}, Rgb{150, 176, 196}, value_noise(w, 20, 3));
                    if (hash01(uint32_t(cell + 4096), uint32_t(std::floor(along / 32) + 64), 8) < 0.42 && f < 0.3) band = mix(band, kWhite, 0.55);
                    if (post < 0.9) band = Rgb{60, 76, 96};
                    alpha = post < 0.9 ? 1 : 0.8;
                }
                if (dO > -2.2) band = Rgb{206, 218, 230}, alpha = 1;  // rail
                if (dO > -0.7) band = kInk;
                c = near_board || dI * scale > 0.5 ? band : mix(icec, band, clamp01(0.5 + dI * scale));
                if (!near_board && dI * scale <= 0.5) alpha = 1;
            }
            s.blend(x, y, c, std::min(cover, 1.0) * alpha);
        }
    {  // the home colour at centre ice
        const Vec c{kMidX, kMidY};
        p.circle(c, 34, home_, 0.5);
        p.ring(c, 34, 1.6, kWhite, 0.85);
        p.ring(c, 29.5, 0.7, kWhite, 0.6);
        draw_text(p, "EM", {kMidX, kMidY - 11.5}, {1, 0}, {0, 1}, 23, 1, kWhite, 1.15, 0.92);
    }
    for (int i = 0; i < kAdCount; i++) {  // advertising on the far board
        const double room = kAdWidth - 7;
        double cap = 5.4;
        if (text_width(kAds[i].name, cap) > room) cap *= room / text_width(kAds[i].name, cap);
        draw_text(p, kAds[i].name, {kAdLeft + kAdWidth * (i + 0.5), 76.4 - cap / 2}, {1, 0}, {0, 1}, cap, 1, kAds[i].ink, 1.05);
    }
    paint_crease(p, 128.5, -1);
    paint_crease(p, 1023, 1);
    paint_ice_items(p);
}

}  // namespace em4k
