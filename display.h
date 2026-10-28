#pragma once
#include <opxy/page_header.hpp>
#include "mandala.h"

namespace mi_rings {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// The node map, shared with tools/gen_panel.py:
//   0..7 the header bar; 8 the body circle; 9..11 its inner rings; 12 the centre
//   dot; 13 the pluck point; 14..16 ripples; then the string body's dashes,
//   the sympathetic body's waves and the chords body's lines; the four
//   mandalas, one per model, laid out in mandala.h (generated with the
//   artwork); the lit model LED, the lit modes LED and the two halos.
using Frame = opxy::draw::Frame<0, 0, kNodes, 16, 16>;
const opxy::u32 kBodyNode = 8, kFirstInner = 9, kInnerRings = 3, kCentreNode = 12, kStrikeNode = 13;
const opxy::u32 kFirstRipple = 14, kRipples = 3, kWaves = 4;
const opxy::u32 kLayers = 7;
struct DisplayState {
    opxy::u32 initialized, track, previous_event;
    opxy::i32 age;                      // since the last note, Q10 seconds, saturating
    opxy::i32 ripple_age[kRipples];
    opxy::u32 next_ripple;
    opxy::i32 wave_age[kWaves];         // the mandala's light ripples, one per recent note
    opxy::u32 next_wave;
    opxy::i32 angle;                    // the mandala's turn, tenths of a degree, 0..3599
    opxy::u32 model, modes;             // the lit LEDs
    opxy::i32 halo_age[2];              // since the model and modes last changed, Q10 seconds
    opxy::draw::PageHeader header;
};
}
