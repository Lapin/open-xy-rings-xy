#pragma once
#include <opxy/page_header.hpp>

namespace mi_rings {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// The node map, shared with tools/gen_panel.py:
//   0..7 the header bar; 8 the body circle; 9..11 its inner rings; 12 the centre
//   dot; 13 the pluck point; 14..16 ripples; 17..20 strings; 21..32, 33..48 and
//   49..72 the mandala's three rings of petals; 73..76 the model LEDs; 77..80 the
//   modes LEDs; 81 and 82 the halos that mark a change.
using Frame = opxy::draw::Frame<0, 0, 83, 16, 16>;
const opxy::u32 kBodyNode = 8, kFirstInner = 9, kInnerRings = 3, kCentreNode = 12, kStrikeNode = 13;
const opxy::u32 kFirstRipple = 14, kRipples = 3, kFirstString = 17, kStrings4 = 4;
const opxy::u32 kFirstA = 21, kPetalsA = 12, kFirstB = 33, kPetalsB = 16, kFirstC = 49, kPetalsC = 24;
const opxy::u32 kFirstModelLed = 73, kFirstModesLed = 77, kHaloNode = 81;
struct DisplayState {
    opxy::u32 initialized, track, previous_event;
    opxy::i32 age;                      // since the last note, Q10 seconds, saturating
    opxy::i32 ripple_age[kRipples];
    opxy::u32 next_ripple;
    opxy::i32 angle;                    // the mandala's turn, tenths of a degree, 0..3599
    opxy::u32 model, modes;             // the lit LEDs
    opxy::i32 halo_age[2];              // since the model and modes last changed, Q10 seconds
    opxy::draw::PageHeader header;
};
}
