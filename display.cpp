// The screen: the shared two-page header, and under it the partial series the
// note's modes sit on: a spike per partial, left to right by pitch (an octave
// every 56 pixels), as tall as Position lets it ring.
#include "display.h"
#include "series.h"

namespace mi_rings {
namespace {
// log2 of a positive word, Q10, from its top bit and a bent mantissa.
inline opxy::i32 log2_q10(opxy::u32 value) {
    opxy::i32 exponent = 0;
    opxy::u32 m = value;
    if (m & 0xffff0000u) { m >>= 16; exponent += 16; }
    if (m & 0xff00u) { m >>= 8; exponent += 8; }
    if (m & 0xf0u) { m >>= 4; exponent += 4; }
    if (m & 0xcu) { m >>= 2; exponent += 2; }
    if (m & 0x2u) { exponent += 1; }
    const opxy::u32 mantissa = exponent >= 10 ? (value >> (exponent - 10)) & 1023u : (value << (10 - exponent)) & 1023u;
    const opxy::i32 f = static_cast<opxy::i32>(mantissa);
    return (exponent << 10) + f + ((f * (1024 - f)) * 11 >> 15);  // log2(1 + f) ~ f + 0.34 f (1 - f)
}
}

inline void display_reset(DisplayState& state) {
    state.key = 0;
    state.structure = state.position = -1;
    state.count = 0;
    state.header.reset();
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace opxy::draw;
    const char* const titles[8] = {"Struct", "Bright", "Damping", "Position", "Modes", "Strike", "Damping", "Position"};
    char values[8][header_capacity];
    for (opxy::u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    const opxy::u32 count = mode_count(input.slots[4]);
    values[4][0] = static_cast<char>('0' + count / 10);
    values[4][1] = static_cast<char>('0' + count % 10);
    values[4][2] = 0;
    if (count < 10) { values[4][0] = values[4][1]; values[4][1] = 0; }
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 2, 3}};
    page_header(state.header, input, frame, titles, values, 2, slots);

    if (state.key == 0 || input.slots[0] != state.structure || input.slots[3] != state.position || count != state.count) {
        state.structure = input.slots[0];
        state.position = input.slots[3];
        state.count = count;
        state.key = state.key + 1 ? state.key + 1 : 1;
        Series series;
        compute_series(input.slots[0], input.slots[3], count, series);
        // Each path is a thin closed ribbon along one half of the screen, 240 px
        // at scale 2 around its node: out along the baseline with a spike for
        // every partial that lands in it, back a quarter pixel below.
        const opxy::i32 edge = unit / 8;  // half a spike's width, locally
        opxy::u32 used[2] = {1, 1};
        for (opxy::u32 p = 0; p < 2; ++p) {
            frame.paths[p].key = state.key;
            frame.paths[p].points[0].x = -60 * unit;
            frame.paths[p].points[0].y = 0;
        }
        for (opxy::u32 i = 0; i < series.count; ++i) {
            // (i + 1) * stretch in octaves above the note: -1 .. 7 across 24 .. 472 px.
            const opxy::i32 octaves = log2_q10(static_cast<opxy::u32>(series.stretch[i]) * (i + 1)) - (25 << 10);
            const opxy::i32 x_px = 24 + ((octaves + 1024) * 56 >> 10);
            if (x_px < 1 || x_px >= 479) continue;
            const opxy::u32 p = x_px < 240 ? 0 : 1;
            if (used[p] + 4 > Frame::point_count - 3) continue;
            const opxy::i32 x = (x_px - (120 + 240 * static_cast<opxy::i32>(p))) * (unit / 2);
            const opxy::i32 height = ((series.amplitude[i] >> 23) * 25 >> 8) * unit + unit / 2;  // 0.5..25.5 local px
            Point* at = &frame.paths[p].points[used[p]];
            at[0].x = x - edge; at[0].y = 0;
            at[1].x = x - edge; at[1].y = -height;
            at[2].x = x + edge; at[2].y = -height;
            at[3].x = x + edge; at[3].y = 0;
            used[p] += 4;
        }
        for (opxy::u32 p = 0; p < 2; ++p) {
            Point* points = frame.paths[p].points;
            opxy::u32 n = used[p];
            points[n].x = 60 * unit;              points[n].y = 0;
            points[n + 1].x = 60 * unit;          points[n + 1].y = unit / 4;
            points[n + 2].x = -60 * unit;         points[n + 2].y = unit / 4;
            for (n += 3; n < Frame::point_count; ++n) {
                // Spare points sit on the way home, each a step before the last.
                points[n].x = -60 * unit + static_cast<opxy::i32>(Frame::point_count - n) * (unit / 16);
                points[n].y = unit / 4;
            }
        }
    }
    place(frame, 0, 120 * unit, 150 * unit, 2 * unit, 15, stroke);
    place(frame, 1, 360 * unit, 150 * unit, 2 * unit, 15, stroke);
}
}
