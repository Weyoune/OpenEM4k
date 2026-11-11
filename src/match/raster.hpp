// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Small software vector rasteriser of the new match scenes: shapes are signed distance functions in world units
// (rink pixels of the original, 672 x 1152), painted with analytic antialiasing at any scale. No SDL.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace em4k {

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
};
constexpr Rgb mix(Rgb a, Rgb b, double t) {
    return {uint8_t(a.r + (b.r - a.r) * t + 0.5), uint8_t(a.g + (b.g - a.g) * t + 0.5), uint8_t(a.b + (b.b - a.b) * t + 0.5)};
}
constexpr Rgb shade(Rgb c, double f) {  // f < 1 darker, f > 1 towards white
    return f <= 1 ? mix(Rgb{0, 0, 0}, c, f) : mix(c, Rgb{255, 255, 255}, f - 1);
}

// Opaque picture, 0xFFRRGGBB per pixel (SDL_PIXELFORMAT_ARGB8888)
struct Surface {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;

    Surface() = default;
    Surface(int w, int h, Rgb fill = {}) : width(w), height(h), pixels(size_t(w) * size_t(h), pack(fill)) {}
    static uint32_t pack(Rgb c) { return 0xFF000000u | uint32_t(c.r) << 16 | uint32_t(c.g) << 8 | c.b; }
    static Rgb unpack(uint32_t v) { return {uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; }
    void blend(int x, int y, Rgb c, double a) {
        uint32_t& p = pixels[size_t(y) * size_t(width) + size_t(x)];
        if (a >= 0.999) {
            p = pack(c);
            return;
        }
        const int k = int(a * 256 + 0.5);
        const uint32_t r = ((p >> 16 & 255) * uint32_t(256 - k) + c.r * uint32_t(k)) >> 8;
        const uint32_t g = ((p >> 8 & 255) * uint32_t(256 - k) + c.g * uint32_t(k)) >> 8;
        const uint32_t b = ((p & 255) * uint32_t(256 - k) + c.b * uint32_t(k)) >> 8;
        p = 0xFF000000u | r << 16 | g << 8 | b;
    }
};

struct Vec {
    double x = 0, y = 0;
};
inline Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y}; }
inline Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y}; }
inline Vec operator*(Vec a, double k) { return {a.x * k, a.y * k}; }
inline double dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y; }
inline double length(Vec a) { return std::sqrt(dot(a, a)); }
inline Vec lerp(Vec a, Vec b, double t) { return a + (b - a) * t; }
inline Vec normal(Vec a) {
    const double l = length(a);
    return l > 1e-9 ? a * (1 / l) : Vec{1, 0};
}
inline Vec perp(Vec a) { return {-a.y, a.x}; }

// Signed distances (negative inside)
inline double sd_circle(Vec p, Vec c, double r) { return length(p - c) - r; }
inline double sd_capsule(Vec p, Vec a, Vec b, double ra, double rb) {
    const Vec ab = b - a, ap = p - a;
    const double l2 = dot(ab, ab);
    const double t = l2 > 1e-12 ? std::clamp(dot(ap, ab) / l2, 0.0, 1.0) : 0.0;
    return length(ap - ab * t) - (ra + (rb - ra) * t);
}
inline double sd_ellipse(Vec p, Vec c, double rx, double ry, double cos_a = 1, double sin_a = 0) {
    const Vec d = p - c;
    const Vec q{d.x * cos_a + d.y * sin_a, -d.x * sin_a + d.y * cos_a};
    const double k1 = std::sqrt(q.x * q.x / (rx * rx) + q.y * q.y / (ry * ry));
    const double k2 = std::sqrt(q.x * q.x / (rx * rx * rx * rx) + q.y * q.y / (ry * ry * ry * ry));
    return k2 > 1e-12 ? k1 * (k1 - 1) / k2 : -std::min(rx, ry);
}
inline double sd_box(Vec p, Vec c, double hx, double hy, double r = 0, double cos_a = 1, double sin_a = 0) {
    const Vec d = p - c;
    const double qx = std::abs(d.x * cos_a + d.y * sin_a) - (hx - r), qy = std::abs(-d.x * sin_a + d.y * cos_a) - (hy - r);
    const double ox = std::max(qx, 0.0), oy = std::max(qy, 0.0);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0) - r;
}
template <size_t N>
double sd_polygon(Vec p, const Vec (&v)[N]) {
    double d = dot(p - v[0], p - v[0]);
    bool inside = false;
    for (size_t i = 0, j = N - 1; i < N; j = i, i++) {
        const Vec e = v[j] - v[i], w = p - v[i];
        const Vec b = w - e * std::clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
        d = std::min(d, dot(b, b));
        const bool c1 = p.y >= v[i].y, c2 = p.y < v[j].y, c3 = e.x * w.y > e.y * w.x;
        if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) inside = !inside;
    }
    return inside ? -std::sqrt(d) : std::sqrt(d);
}

// Paints in world coordinates: pixel = (world - origin) * scale.
struct Painter {
    Surface& target;
    double scale = 1;
    Vec origin{};
    int clip_top = 0, clip_bottom = 1 << 30;  // rows painted: lets several threads share one picture in bands

    Painter moved(Vec by) const { return {target, scale, origin - by, clip_top, clip_bottom}; }

    // Every pixel of the world rectangle: f(world position) -> coverage in 0..1 of colour `c`.
    template <class F>
    void cover(double x0, double y0, double x1, double y1, Rgb c, F coverage) {
        const int px0 = std::max(0, int(std::floor((x0 - origin.x) * scale))), py0 = std::max(clip_top, int(std::floor((y0 - origin.y) * scale)));
        const int px1 = std::min(target.width, int(std::ceil((x1 - origin.x) * scale)) + 1);
        const int py1 = std::min(std::min(target.height, clip_bottom), int(std::ceil((y1 - origin.y) * scale)) + 1);
        for (int y = py0; y < py1; y++)
            for (int x = px0; x < px1; x++) {
                const double a = coverage(Vec{origin.x + (x + 0.5) / scale, origin.y + (y + 0.5) / scale});
                if (a > 0.004) target.blend(x, y, c, std::min(a, 1.0));
            }
    }
    // Shape given by a signed distance function, with an optional outline of `line` world units around it.
    template <class F>
    void shape(double x0, double y0, double x1, double y1, F sdf, Rgb fill, double alpha = 1, double line = 0, Rgb line_colour = {}) {
        const double pad = line + 1 / scale;
        const int px0 = std::max(0, int(std::floor((x0 - pad - origin.x) * scale))), py0 = std::max(clip_top, int(std::floor((y0 - pad - origin.y) * scale)));
        const int px1 = std::min(target.width, int(std::ceil((x1 + pad - origin.x) * scale)) + 1);
        const int py1 = std::min(std::min(target.height, clip_bottom), int(std::ceil((y1 + pad - origin.y) * scale)) + 1);
        for (int y = py0; y < py1; y++)
            for (int x = px0; x < px1; x++) {
                const double d = sdf(Vec{origin.x + (x + 0.5) / scale, origin.y + (y + 0.5) / scale}) * scale;
                if (line > 0) {
                    const double o = 0.5 - (d - line * scale);
                    if (o > 0.004) target.blend(x, y, line_colour, std::min(o, 1.0) * alpha);
                }
                const double a = 0.5 - d;
                if (a > 0.004) target.blend(x, y, fill, std::min(a, 1.0) * alpha);
            }
    }

    void circle(Vec c, double r, Rgb fill, double alpha = 1, double line = 0, Rgb lc = {}) {
        shape(c.x - r, c.y - r, c.x + r, c.y + r, [=](Vec p) { return sd_circle(p, c, r); }, fill, alpha, line, lc);
    }
    void ring(Vec c, double r, double width, Rgb fill, double alpha = 1) {
        const double o = r + width;
        shape(c.x - o, c.y - o, c.x + o, c.y + o, [=](Vec p) { return std::abs(sd_circle(p, c, r)) - width / 2; }, fill, alpha);
    }
    void capsule(Vec a, Vec b, double ra, double rb, Rgb fill, double alpha = 1, double line = 0, Rgb lc = {}) {
        const double r = std::max(ra, rb);
        shape(std::min(a.x, b.x) - r, std::min(a.y, b.y) - r, std::max(a.x, b.x) + r, std::max(a.y, b.y) + r,
              [=](Vec p) { return sd_capsule(p, a, b, ra, rb); }, fill, alpha, line, lc);
    }
    void line(Vec a, Vec b, double width, Rgb fill, double alpha = 1) { capsule(a, b, width / 2, width / 2, fill, alpha); }
    void ellipse(Vec c, double rx, double ry, double angle, Rgb fill, double alpha = 1, double line = 0, Rgb lc = {}) {
        const double ca = std::cos(angle), sa = std::sin(angle), r = std::max(rx, ry);
        shape(c.x - r, c.y - r, c.x + r, c.y + r, [=](Vec p) { return sd_ellipse(p, c, rx, ry, ca, sa); }, fill, alpha, line, lc);
    }
    void box(Vec c, double hx, double hy, double radius, double angle, Rgb fill, double alpha = 1, double line = 0, Rgb lc = {}) {
        const double ca = std::cos(angle), sa = std::sin(angle);
        const double ex = std::abs(hx * ca) + std::abs(hy * sa), ey = std::abs(hx * sa) + std::abs(hy * ca);  // extent of the turned box
        shape(c.x - ex, c.y - ey, c.x + ex, c.y + ey, [=](Vec p) { return sd_box(p, c, hx, hy, radius, ca, sa); }, fill, alpha, line, lc);
    }
    // Soft blob without an edge (light on the ice, shadows, glow): full `alpha` in the middle, nothing at the radii.
    void soft(Vec c, double rx, double ry, Rgb fill, double alpha) {
        cover(c.x - rx, c.y - ry, c.x + rx, c.y + ry, fill, [=](Vec p) {
            const double dx = (p.x - c.x) / rx, dy = (p.y - c.y) / ry, d = 1 - dx * dx - dy * dy;
            return d <= 0 ? 0.0 : alpha * d * d;
        });
    }
    void rect(double x0, double y0, double x1, double y1, Rgb fill, double alpha = 1) {
        const Vec c{(x0 + x1) / 2, (y0 + y1) / 2};
        const double hx = (x1 - x0) / 2, hy = (y1 - y0) / 2;
        shape(x0, y0, x1, y1, [=](Vec p) { return sd_box(p, c, hx, hy); }, fill, alpha);
    }
};

// Deterministic noise for textures and placement
inline uint32_t hash32(uint32_t a, uint32_t b = 0, uint32_t c = 0) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu ^ (c + 0x165667B1u) * 0xC2B2AE35u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}
inline double hash01(uint32_t a, uint32_t b = 0, uint32_t c = 0) { return hash32(a, b, c) / 4294967296.0; }

}  // namespace em4k
