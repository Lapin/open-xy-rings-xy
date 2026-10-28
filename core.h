#pragma once
#include <opxy/audio.hpp>
#include "series.h"

namespace mi_rings {
// The resonator's signal format: 1.0 is 1 << 24, leaving 128x headroom for the
// sum of the modes before the limiter.
const opxy::i32 kUnity = 1 << 24;

// One mode: a two-pole resonator in the magic-circle form, which keeps its
// frequency precise at low pitches. Its loss is applied every sample when the
// decay is short, or once per block with an exact fraction carry when it is
// long, so quiet tails keep decaying instead of freezing on truncation.
struct Mode {
    opxy::i32 x, y;              // state, Q7.24
    opxy::i32 rotation;          // sin(pi f): half the rotation coefficient, Q31
    opxy::i32 drive;             // the exciter's gain into this mode, (pi/2) f, Q31
    opxy::i32 amplitude;         // Position's weight, Q31
    opxy::i32 decay;             // heavy: 1 - loss per sample; light: loss per block (Q31)
    opxy::u32 carry_x, carry_y;  // fractions of the per-block loss not yet applied
    opxy::u32 heavy;             // 1: loss every sample
    opxy::u32 active;            // 0: above the audible band, or beyond Modes
};

// The strike: a pulse, or a short noise burst, through a resonant low-pass
// whose cutoff follows Bright around the note.
struct Exciter {
    opxy::i32 s1, s2;      // low-pass state, Q7.24
    opxy::i32 g, h;        // low-pass coefficients, Q31
    opxy::i32 pulse;       // the strike's energy as it enters the filter, Q7.24
    opxy::u32 burst;       // strike samples left
    opxy::u32 noise;       // 1: the burst is noise; 0: one pulse
    opxy::u32 active;      // the filter still rings
};

struct Voice {
    Mode modes[kMaxModes];
    Exciter exciter;
    opxy::u32 count;                                   // modes the series ends at
    opxy::i32 structure, brightness, damping, position;  // controls the modes were tuned for
    opxy::u32 modes_control, pitch;
    opxy::u32 seed;                                    // noise
    opxy::u32 pending;                                 // a strike waits for the next block
    opxy::i32 peak;                                    // the limiter's followed peak, Q7.24
};
// State shared by every voice; empty when voices share nothing.
struct Shared {};
}
