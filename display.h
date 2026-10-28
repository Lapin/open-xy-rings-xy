#pragma once
#include <opxy/page_header.hpp>

namespace mi_rings {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// The node map, shared with tools/gen_panel.py:
//   0..7 the header bar; 8 the body circle; 9..11 its inner rings; 12 the centre
//   dot; 13 the pluck point; 14..16 ripples; 17..20 strings; then the mandala
//   lifted from the Rings panel: 21..32 the paisleys' outlines, 33..44 their
//   inner teardrops, 45..56 eyes, 57..68 pupils, 69..80 small drops, 81 and 82
//   the wavy rings, 83..94 the circled dots' rings and 95..106 their dots;
//   107..110 the model LEDs; 111..114 the modes LEDs; 115 and 116 the halos.
using Frame = opxy::draw::Frame<0, 0, 117, 16, 16>;
const opxy::u32 kBodyNode = 8, kFirstInner = 9, kInnerRings = 3, kCentreNode = 12, kStrikeNode = 13;
const opxy::u32 kFirstRipple = 14, kRipples = 3, kFirstString = 17, kStrings4 = 4;
const opxy::u32 kPaisleys = 12, kFirstOutline = 21, kFirstInnerDrop = 33, kFirstEye = 45, kFirstPupil = 57, kFirstDrop = 69;
const opxy::u32 kRingInner = 81, kRingOuter = 82, kFirstDotRing = 83, kFirstDot = 95;
const opxy::u32 kFirstModelLed = 107, kFirstModesLed = 111, kHaloNode = 115;
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
