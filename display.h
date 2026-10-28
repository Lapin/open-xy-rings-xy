#pragma once
#include <opxy/page_header.hpp>

namespace mi_rings {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<2, 64, 10, 16, 16>;
struct DisplayState {
    opxy::u32 key;                      // the partial picture's geometry key; 0 before the first draw
    opxy::i32 structure, position;      // the controls the picture shows
    opxy::u32 count;
    opxy::draw::PageHeader header;
};
}
