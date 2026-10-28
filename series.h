#pragma once
// The partial series a note's modes sit on, shared by the sound and the screen.
// Structure stretches the series the way Mutable Instruments Rings does
// (rings/dsp/resonator.cc, MIT, Emilie Gillet); Position weights the partials
// like a strike point along a string.
#include <opxy/types.hpp>
#include <opxy/q31.hpp>
#include "tables.h"

namespace mi_rings {
const unsigned kMaxModes = 24;
const unsigned kModeChoices = 4;
const unsigned kModeCounts[kModeChoices] = {1, 3, 8, 24};  // the Size enum: bare, lean, full, lush

// Interpolated read of a 257-entry table over a normalized Q31 control.
inline opxy::i32 lookup(const opxy::i32* table, opxy::i32 control) {
    const opxy::u32 c = control < 0 ? 0u : static_cast<opxy::u32>(control);
    const opxy::u32 index = c >> 23;                                                 // 0..255
    const opxy::i32 fraction = static_cast<opxy::i32>((c << 8) & 0x7fffffffu);     // Q31, within the step
    const opxy::i32 a = table[index], b = table[index + 1];
    return a + opxy::q31::multiply(b - a, fraction);
}

// sin(pi/2 * t), t in [0, 1] as Q31; Q31 result.
inline opxy::i32 sine_quarter(opxy::i32 t) { return lookup(kSine, t); }

// cos(2 pi * phase), phase in [0, 1) as a 31-bit fraction; Q31 result.
inline opxy::i32 cosine(opxy::u32 phase) {
    const opxy::u32 quadrant = phase >> 29;
    const opxy::i32 t = static_cast<opxy::i32>((phase << 2) & 0x7fffffffu);
    switch (quadrant) {
        case 0: return sine_quarter(0x7fffffff - t);
        case 1: return -sine_quarter(t);
        case 2: return -sine_quarter(0x7fffffff - t);
        default: return sine_quarter(t);
    }
}

// The Size enum choice from its Q31 word (stock stores the middle of a choice).
inline opxy::u32 mode_choice(opxy::i32 control) {
    const opxy::u32 c = control < 0 ? 0u : static_cast<opxy::u32>(control);
    const opxy::u32 choice = (c >> 16) * kModeChoices >> 15;
    return choice >= kModeChoices ? kModeChoices - 1 : choice;
}
// On the sympathetic models Size is how many strings ring, 1 to 4: the note
// and up to three more, as Rings trades strings for voices.
inline opxy::u32 string_count(opxy::i32 control) { return mode_choice(control) + 1; }
// The modal body's partial count.
inline opxy::u32 mode_count(opxy::i32 control) { return kModeCounts[mode_choice(control)]; }
// How far lush eases Rings' fall of Q up the partials, so its top partials
// ring at any Bright: at Bright 50 Rings leaves nothing above the eighth, and
// full and lush would be the same sound. Q31.
const opxy::i32 kModeEase[kModeChoices] = {0, 0, 0, 1610612736};  // 0, 0, 0, 0.75
inline opxy::i32 mode_ease(opxy::i32 control) { return kModeEase[mode_choice(control)]; }

struct Series {
    opxy::u32 count;
    opxy::i32 stretch[kMaxModes];    // partial i sits at (i + 1) * stretch[i] times the note; Q25
    opxy::i32 amplitude[kMaxModes];  // Position's weight of partial i; Q31
};

// structure and position are Q31 controls; position covers half a strike cycle,
// so 0 hears every partial equally and 1 is a strike at the middle.
inline void compute_series(opxy::i32 structure, opxy::i32 position, opxy::u32 count, Series& series) {
    using opxy::i32;
    using opxy::q31::multiply;
    const i32 kShrink = 1997159792;  // 0.93: a negative stiffness fades faster
    const i32 kGrow = 2104533974;    // 0.98
    const i32 kStretchMax = 48 << 25, kStretchMin = 1 << 20;  // 48 and 1/32; a step is at most 2^26
    i32 stiffness = lookup(kStiffness, structure);  // Q27
    i32 stretch = 1 << 25;
    // amplitude_i = 0.5 + 0.5 cos(2 pi p i), by the cosine recurrence in Q29.
    const i32 cos_theta = cosine(position < 0 ? 0u : static_cast<opxy::u32>(position) >> 1);
    i32 before = cos_theta >> 2;  // cos(-theta)
    i32 c = 1 << 29;              // cos(0)
    if (count > kMaxModes) count = kMaxModes;
    series.count = count;
    for (opxy::u32 i = 0; i < count; ++i) {
        series.stretch[i] = stretch;
        const i32 bounded = c > (1 << 29) - 1 ? (1 << 29) - 1 : c < -(1 << 29) ? -(1 << 29) : c;
        series.amplitude[i] = (1 << 30) + bounded * 2;
        const i32 next = (multiply(cos_theta, c) * 2) - before;
        before = c;
        c = next;
        stretch += stiffness >> 2;
        if (stretch > kStretchMax) stretch = kStretchMax;
        if (stretch < kStretchMin) stretch = kStretchMin;
        stiffness = multiply(stiffness, stiffness < 0 ? kShrink : kGrow);
    }
}
}
