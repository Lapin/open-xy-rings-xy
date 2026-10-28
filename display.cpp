// The screen: the shared two-page header, and under it a square tile of
// concentric rings after the Rings logo. One ring per partial, its radius by
// the partial's pitch, so Structure bends the spacing from bunched harmonics to
// a stretched bell; Position sets the strike point and weights the rings; a
// note sends ripples out from the strike point that Damping lets linger.
#include "display.h"
#include "series.h"

namespace mi_rings {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
const i32 kCentreX = 240 * unit, kCentreY = 130 * unit;
const i32 kInnerRadius = 14 * unit, kOuterRadius = 78 * unit;  // the tile is 172 px across
const i32 kAuthoredRadius = 40;                                 // the ring and ripple circles' r
const i32 kExpired = 4 * unit;                                  // the longest ripple life, seconds

// log2 of a positive word, Q10, from its top bit and a bent mantissa.
inline i32 log2_q10(u32 value) {
    i32 exponent = 0;
    u32 m = value;
    if (m & 0xffff0000u) { m >>= 16; exponent += 16; }
    if (m & 0xff00u) { m >>= 8; exponent += 8; }
    if (m & 0xf0u) { m >>= 4; exponent += 4; }
    if (m & 0xcu) { m >>= 2; exponent += 2; }
    if (m & 0x2u) { exponent += 1; }
    const u32 mantissa = exponent >= 10 ? (value >> (exponent - 10)) & 1023u : (value << (10 - exponent)) & 1023u;
    const i32 f = static_cast<i32>(mantissa);
    return (exponent << 10) + f + ((f * (1024 - f)) * 11 >> 15);  // log2(1 + f) ~ f + 0.34 f (1 - f)
}

// A ripple's life in Q10 seconds from Damping: 0.4 s dry to 2.4 s ringing.
inline i32 ripple_life(i32 damping) { return 410 + ((damping >> 20) * 2048 >> 11); }

// 1024 / life, Q10, from a 16-entry table over the life range: no runtime division.
inline i32 ripple_rate(i32 life) {
    static const i32 rates[17] = {2560, 1911, 1524, 1268, 1086, 949, 843, 759, 690, 632, 584, 542, 506, 475, 447, 422, 400};
    const i32 index = clamp((life - 410) * 16 / 2048, 0, 16);
    return rates[index];
}

inline void rings(DisplayState& state, i32 structure, i32 position, u32 count) {
    state.structure = structure;
    state.position = position;
    state.count = count;
    state.key = state.key + 1 ? state.key + 1 : 1;
    Series series;
    compute_series(structure, position, count, series);
    for (u32 i = 0; i < kRings; ++i) {
        state.radius[i] = 0;
        state.weight[i] = 0;
        if (i >= series.count) continue;
        // (i + 1) * stretch in octaves above the note, 12 px an octave from the inner ring.
        const i32 octaves = log2_q10(static_cast<u32>(series.stretch[i]) * (i + 1)) - (25 << 10);
        const i32 radius = kInnerRadius + octaves * 12;
        if (radius < kInnerRadius - 2 * unit || radius > kOuterRadius) continue;
        state.radius[i] = clamp(radius, kInnerRadius - 2 * unit, kOuterRadius);
        state.weight[i] = static_cast<u32>(series.amplitude[i] >> 23);
    }
}
}

inline void display_reset(DisplayState& state) {
    state.key = 0;
    state.structure = state.position = -1;
    state.count = 0;
    state.initialized = state.track = state.previous_event = 0;
    state.age = kExpired;
    for (u32 i = 0; i < kRipples; ++i) state.ripple_age[i] = kExpired;
    state.next_ripple = 0;
    state.header.reset();
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    const char* const titles[8] = {"Struct", "Bright", "Damping", "Position", "Modes", "Strike", "Damping", "Position"};
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    const u32 count = mode_count(input.slots[4]);
    values[4][0] = static_cast<char>('0' + count / 10);
    values[4][1] = static_cast<char>('0' + count % 10);
    values[4][2] = 0;
    if (count < 10) { values[4][0] = values[4][1]; values[4][1] = 0; }
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 2, 3}};
    page_header(state.header, input, frame, titles, values, 0, slots);

    const i32 structure = clamp(input.slots[0], 0, opxy::q31::maximum);
    const i32 brightness = clamp(input.slots[1], 0, opxy::q31::maximum);
    const i32 damping = clamp(input.slots[2], 0, opxy::q31::maximum);
    const i32 position = clamp(input.slots[3], 0, opxy::q31::maximum);
    const i32 strike = clamp(input.slots[5], 0, opxy::q31::maximum);
    if (state.key == 0 || structure != state.structure || position != state.position || count != state.count)
        rings(state, structure, position, count);

    // Strikes: a note starts a ripple at the strike point and a flash on the rings.
    const i32 elapsed = clamp(input.elapsed, 0, kExpired);
    const bool fresh = !state.initialized || state.track != input.track;
    if (fresh || input.reduced_motion) {
        state.age = kExpired;
        for (u32 i = 0; i < kRipples; ++i) state.ripple_age[i] = kExpired;
    } else {
        state.age = clamp(state.age + elapsed, 0, kExpired);
        for (u32 i = 0; i < kRipples; ++i) state.ripple_age[i] = clamp(state.ripple_age[i] + elapsed, 0, kExpired);
        if (input.track < 8 && input.note_serial != state.previous_event) {
            state.age = 0;
            state.ripple_age[state.next_ripple] = 0;
            state.next_ripple = (state.next_ripple + 1) % kRipples;
        }
    }
    state.initialized = 1;
    state.track = input.track;
    state.previous_event = input.note_serial;

    // The flash: full at the strike, gone after the ripple life.
    const i32 life = ripple_life(damping);
    const i32 rate = ripple_rate(life);
    const i32 flash = clamp(unit - (state.age * rate >> 10), 0, unit);  // Q10, 1 at the strike
    const i32 bright = 4 + (brightness >> 27);                           // 4..19

    // The strike point: Position runs along the tile from the left edge (0) to
    // the right (1), through the centre at 0.5, where a strike lands mid-string.
    const i32 strike_x = kCentreX - 70 * unit + ((position >> 16) * 140 >> 5);
    const i32 strike_scale = unit + (strike >> 21) * 2 + flash;  // Strike's brightness fattens the dot
    place(frame, kStrikeNode, strike_x, kCentreY, clamp(strike_scale, unit / 2, 2 * unit), 8 + (flash * 7 >> 10), fill_and_stroke);

    // Rings: dim at rest by Position's weight and Brightness, flashing with a strike.
    for (u32 i = 0; i < kRings; ++i) {
        const u32 node = kFirstRing + i;
        if (!state.radius[i]) { hide(frame, node); continue; }
        const i32 rest = static_cast<i32>(state.weight[i]) * bright >> 9;          // 0..9
        const i32 shade = clamp(1 + rest + (flash * (3 + rest) >> 10), 1, 15);
        // The strike swells the rings by up to 4 %, inner rings first.
        const i32 swell = flash * 40 >> 10;
        const i32 radius = state.radius[i] + (state.radius[i] * swell >> 10);
        place(frame, node, kCentreX, kCentreY, clamp(radius / kAuthoredRadius, unit / 4, 2 * unit), static_cast<u32>(shade), stroke);
    }

    // Ripples: circles growing out of the strike point and fading over the ripple life.
    for (u32 i = 0; i < kRipples; ++i) {
        const u32 node = kFirstRipple + i;
        const i32 progress = state.ripple_age[i] * rate >> 10;  // Q10, 1 at the end of life
        if (input.reduced_motion || progress <= 0 || progress >= unit) { hide(frame, node); continue; }
        const i32 radius = 4 * unit + (progress * 76);           // 4..80 px
        const i32 shade = clamp(((unit - progress) * (6 + bright)) >> 10, 0, 15);
        if (!shade) { hide(frame, node); continue; }
        place(frame, node, strike_x, kCentreY, clamp(radius / kAuthoredRadius, unit / 8, 2 * unit), static_cast<u32>(shade), stroke);
    }
}
}
