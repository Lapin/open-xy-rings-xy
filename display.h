#pragma once
#include <opxy/page_header.hpp>
#include "mandala.h"

namespace mi_rings {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// The node map, shared with tools/gen_panel.py and laid out in mandala.h
// (generated with the artwork): the four mutable outlines that draw the
// string, sympathetic and chords bodies' insides; the header bar; the body
// circle, its inner rings and the centre dot; the
// four mandalas, one per model; the lit model LED, the lit modes LED and the
// two halos.
using Frame = opxy::draw::Frame<kPaths, kPathPoints, kNodes, 16, 16>;
const opxy::u32 kWaves = 4;
const opxy::u32 kLayers = 7;
struct DisplayState {
    opxy::u32 initialized, track, previous_event;
    opxy::i32 age;                      // since the last note, Q10 seconds, saturating
    opxy::i32 wave_age[kWaves];         // the mandala's light ripples, one per recent note
    opxy::u32 next_wave;
    opxy::i32 angle;                    // the mandala's turn, tenths of a degree, 0..3599
    opxy::u32 model, modes;             // the lit LEDs
    opxy::i32 halo_age[2];              // since the model and modes last changed, Q10 seconds
    opxy::u32 serial;                   // the mutable outlines' geometry key, bumped when they change
    opxy::draw::PageHeader header;
};
}
