// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Own vector lettering of the new UI: capital letters, digits and some punctuation as strokes on a 4 x 6 grid with
// cut corners, drawn with round pens at any size and in any direction. Lower case is shown as capitals, "ß" as "SS".
#include <string_view>

#include "match/raster.hpp"

namespace em4k {

// Width in pixels of `utf8` at the given height of the capitals.
double text_width(std::string_view utf8, double cap_height);
// align: 0 left, 1 centred, 2 right of x; y is the top of the capitals. weight: pen width in grid units (0.9 regular).
void draw_text(Surface& out, std::string_view utf8, double x, double y, double cap_height, int align, Rgb colour, double weight = 0.9,
               double alpha = 1);
// The same in the world of a painter and turned: `origin` is the top of the capitals at the aligned end, `across` the
// writing direction and `down` the direction from the top of a letter to its foot (unit vectors).
void draw_text(Painter& p, std::string_view utf8, Vec origin, Vec across, Vec down, double cap_height, int align, Rgb colour, double weight = 0.9,
               double alpha = 1);

}  // namespace em4k
