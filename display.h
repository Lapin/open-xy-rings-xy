#pragma once
#include <opxy/page_header.hpp>

namespace mi_rings {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// Nodes: 0..7 the header bar, 8..19 one ring per partial, 20 the strike point, 21..23 ripples.
using Frame = opxy::draw::Frame<0, 0, 24, 16, 16>;
const opxy::u32 kRings = 12, kRipples = 3;
const opxy::u32 kFirstRing = 8, kStrikeNode = kFirstRing + kRings, kFirstRipple = kStrikeNode + 1;
struct DisplayState {
    opxy::u32 key;                      // the ring geometry's key; 0 before the first draw
    opxy::i32 structure, position;      // the controls the rings show
    opxy::u32 count;
    opxy::i32 radius[kRings];           // each ring's radius in pixels, Q10; 0 hides it
    opxy::u32 weight[kRings];           // Position's weight of its partial, 0..255
    opxy::u32 initialized, track, previous_event;
    opxy::i32 age;                      // since the last strike, Q10 seconds, saturating
    opxy::i32 ripple_age[kRipples];
    opxy::u32 next_ripple;
    opxy::draw::PageHeader header;
};
}
