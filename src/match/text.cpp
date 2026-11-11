// SPDX-License-Identifier: GPL-3.0-only
#include "match/text.hpp"

#include <vector>

namespace em4k {
namespace {

struct Glyph {
    char32_t code;
    int width;
    const char* strokes;  // points as two digits "xy" (y down), strokes separated by '|'
};
constexpr Glyph kGlyphs[] = {
    {U'A', 4, "0602204246|0444"},
    {U'B', 4, "06003041423303|3344453606"},
    {U'C', 4, "4130100105163645"},
    {U'D', 4, "06003041453606"},
    {U'E', 4, "40000646|0333"},
    {U'F', 4, "400006|0333"},
    {U'G', 4, "41301001051636454323"},
    {U'H', 4, "0006|4046|0343"},
    {U'I', 2, "1016"},
    {U'J', 4, "4045361605"},
    {U'K', 4, "0006|400346"},
    {U'L', 4, "000646"},
    {U'M', 4, "0600234046"},
    {U'N', 4, "06004640"},
    {U'O', 4, "103041453616050110"},
    {U'P', 4, "06003041423303"},
    {U'Q', 4, "103041453616050110|2446"},
    {U'R', 4, "06003041423303|2346"},
    {U'S', 4, "413010010213334445361605"},
    {U'T', 4, "0040|2026"},
    {U'U', 4, "000516364540"},
    {U'V', 4, "002640"},
    {U'W', 4, "0016233640"},
    {U'X', 4, "0046|4006"},
    {U'Y', 4, "002340|2326"},
    {U'Z', 4, "00400646"},
    {U'0', 4, "103041453616050110|1432"},
    {U'1', 3, "012026|0636"},
    {U'2', 4, "01103041420646"},
    {U'3', 4, "0110304142334445361605|1333"},
    {U'4', 4, "300343|3036"},
    {U'5', 4, "400003334445361605"},
    {U'6', 4, "4130100105163645443303"},
    {U'7', 4, "004016"},
    {U'8', 4, "103041423313020110|1304051636454433"},
    {U'9', 4, "0516364541301001021343"},
    {U'.', 1, "0606"},
    {U',', 1, "0507"},
    {U':', 1, "0202|0505"},
    {U'-', 3, "0333"},
    {U'+', 4, "0343|2125"},
    {U'/', 4, "0640"},
    {U'\'', 1, "0001"},
    {U'!', 1, "0004|0606"},
    {U'?', 4, "01103041422324|2626"},
    {U'(', 2, "20111526"},
    {U')', 2, "00111506"},
    {U'&', 4, "46110010210405163644"},
};
constexpr double kGap = 1.7, kSpace = 2.6;

const Glyph* glyph(char32_t c) {
    for (const Glyph& g : kGlyphs)
        if (g.code == c) return &g;
    return nullptr;
}

struct Letter {
    const Glyph* glyph = nullptr;  // null: space
    bool dots = false;             // umlaut
};
std::vector<Letter> letters(std::string_view utf8) {
    std::vector<Letter> out;
    for (size_t i = 0; i < utf8.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        char32_t code = c;
        bool dots = false;
        if (c == 0xC3 && i + 1 < utf8.size()) {
            const unsigned char d = static_cast<unsigned char>(utf8[++i]);
            if (d == 0x84 || d == 0xA4) code = U'A', dots = true;
            else if (d == 0x96 || d == 0xB6) code = U'O', dots = true;
            else if (d == 0x9C || d == 0xBC) code = U'U', dots = true;
            else if (d == 0x9F) {
                out.push_back({glyph(U'S')});
                code = U'S';
            } else if (d == 0x89 || d == 0xA9 || d == 0x88 || d == 0xA8) code = U'E';
            else code = U'?';
        } else if (c >= 0x80) {
            while (i + 1 < utf8.size() && (static_cast<unsigned char>(utf8[i + 1]) & 0xC0) == 0x80) i++;
            code = U'?';
        } else if (c >= 'a' && c <= 'z') {
            code = char32_t(c - 'a' + 'A');
        }
        out.push_back({code == U' ' ? nullptr : glyph(code) ? glyph(code) : glyph(U'?'), dots});
        if (code == U' ') out.back().glyph = nullptr;
    }
    return out;
}

double units(const std::vector<Letter>& text) {
    double w = 0;
    for (size_t i = 0; i < text.size(); i++) w += (text[i].glyph ? text[i].glyph->width : kSpace) + (i + 1 < text.size() ? kGap : 0);
    return w;
}

}  // namespace

double text_width(std::string_view utf8, double cap_height) { return units(letters(utf8)) * cap_height / 6; }

void draw_text(Painter& p, std::string_view utf8, Vec origin, Vec across, Vec down, double cap_height, int align, Rgb colour, double weight, double alpha) {
    const std::vector<Letter> text = letters(utf8);
    const double unit = cap_height / 6;
    if (unit <= 0) return;
    double pen = -(align == 1 ? units(text) / 2 : align == 2 ? units(text) : 0);
    const double r = weight * unit / 2;
    const auto at = [&](double gx, double gy) { return origin + across * ((pen + gx) * unit) + down * (gy * unit); };
    for (const Letter& l : text) {
        if (!l.glyph) {
            pen += kSpace + kGap;
            continue;
        }
        const char* s = l.glyph->strokes;
        Vec last;
        bool open = false;
        for (; *s; s++) {
            if (*s == '|') {
                open = false;
                continue;
            }
            const Vec point = at(double(s[0] - '0'), double(s[1] - '0'));
            s++;
            if (open) p.capsule(last, point, r, r, colour, alpha);
            else if (s[1] == '|' || s[1] == 0) p.circle(point, r, colour, alpha);
            last = point;
            open = true;
        }
        if (l.dots) {
            const double mid = l.glyph->width / 2.0;
            p.circle(at(mid - 1.1, -1.5), r * 1.1, colour, alpha);
            p.circle(at(mid + 1.1, -1.5), r * 1.1, colour, alpha);
        }
        pen += l.glyph->width + kGap;
    }
}

void draw_text(Surface& out, std::string_view utf8, double x, double y, double cap_height, int align, Rgb colour, double weight, double alpha) {
    Painter p{out, 1, {0, 0}};
    draw_text(p, utf8, {x, y}, {1, 0}, {0, 1}, cap_height, align, colour, weight, alpha);
}

}  // namespace em4k
