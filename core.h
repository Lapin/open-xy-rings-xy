#pragma once
#include <opxy/audio.hpp>
#include "series.h"

namespace rings_xy {
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

// Rings' models: the modal bank above, or Karplus-Strong strings after
// rings/dsp/string.cc: one string with dispersion, or a main string with
// three sympathetic strings tuned by Structure, freely or to a chord.
enum Model { kModal = 0, kString = 1, kSympathetic = 2, kChords = 3 };
const unsigned kModels = 4;
const unsigned kLineLength = 2048;    // the longest loop: 21.5 Hz at 44.1 kHz
const unsigned kStretchLength = 1024;
const unsigned kCombLength = 256;
const unsigned kStrings = 4;          // the main string and three sympathetic ones

// A string's loop, read back a fractional delay behind its write point.
struct Line { opxy::u32 write; opxy::i32 samples[kLineLength]; };
struct Stretch { opxy::u32 write; opxy::i32 samples[kStretchLength]; };

struct String {
    opxy::u32 increment;         // the string's pitch, Q32 cycles per sample, after glide
    opxy::u32 target;            // where glide is heading
    opxy::i32 delay;             // the loop length the last block ended at, Q16 samples
    opxy::i32 x1, x2;            // the damping FIR's memory, Q7.24
    opxy::i32 s1, s2;            // the damping low-pass's state
    opxy::i32 dc_x, dc_y;        // the bridge's DC blocker
    opxy::i32 noise, bridge;     // dispersion noise, and the curved bridge's last bend, Q7.24
    opxy::i32 c0, c1;            // damping FIR taps, Q31
    opxy::i32 g, h, gh;          // damping low-pass coefficients and their product, Q31
    opxy::i32 comb;              // Position's pickup, as a fraction of the loop, Q31
    opxy::i32 pickup_mix;        // how much the pickup subtracts, Q31: faded in over the first quarter of Position
    opxy::i32 ap_gain, stretch_point, noise_amount, bridge_curving, ac_blocking, noise_filter, noise_gain;
    opxy::u32 active;            // rendered last block; a string coming back starts from silence
    Line line;
};

// Rings' plucker: a noise burst through a comb filter at Position's point,
// then a low-pass that Bright sets, exciting the main string.
struct Plucker {
    opxy::u32 remaining;         // noise samples left in the burst
    opxy::i32 period;            // the comb's delay, Q16 samples
    opxy::i32 gain;              // the comb's feedback, Q31
    opxy::i32 burst;             // the noise's gain, Q28: short bursts lifted
    opxy::i32 g, h, gh, s1, s2;  // the low-pass, with g h as one coefficient
    opxy::u32 write;
    opxy::u32 spent;             // the burst over and the comb rung down: silence until the next note
    opxy::i32 comb[kCombLength];
};

struct Voice {
    Mode modes[kMaxModes];
    Exciter exciter;
    opxy::u32 count;                                   // modes the series ends at
    opxy::i32 structure, brightness, damping, position;  // controls the voice was tuned for
    opxy::u32 modes_control, pitch, model;
    opxy::u32 seed;                                    // noise
    opxy::u32 pending;                                 // a strike waits for the next block
    opxy::i32 peak;                                    // the limiter's followed peak, Q7.24
    // Strings
    String strings[kStrings];
    Stretch stretch;                                   // the main string's dispersion all-pass
    Plucker plucker;
    opxy::i32 in_dc_x, in_dc_y;                        // the exciter's DC blocker
    opxy::u32 ringing;                                 // strings still carry sound
    opxy::u32 gliding;                                 // sympathetic strings still moving to their notes
};
// State shared by every voice; empty when voices share nothing.
struct Shared {};
}
