// The screen: the shared two-page header; the model and modes LEDs either side,
// as on the Rings panel, lit by the choice and haloed when it changes; and in
// the middle the body, a circle with a dot at its centre, inside a mandala of
// petals that turns slowly, kicks on every note and eases back as the note
// fades. Structure adds petals, Bright lights the body's inner rings or strings,
// Damping sets how long the kick lasts, Position places the pluck point and the
// ripples that spread from it.
#include "display.h"
#include "series.h"

namespace mi_rings {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
const i32 kCentreX = 240 * unit, kCentreY = 130 * unit;
const i32 kBodyRadius = 42 * unit;
const i32 kAuthoredRipple = 40;                                 // the ripple circles' r
const i32 kExpired = 4 * unit;                                  // the longest kick, seconds
const i32 kHaloLife = unit;                                     // a change's halo, one second
const i32 kRestSpeed = 40;                                      // tenths of a degree per second at rest
const i32 kKickSpeed = 700;                                     // added by a fresh note
const i32 kLedX[2] = {30 * unit, 450 * unit};
const i32 kLedY[4] = {82 * unit, 114 * unit, 146 * unit, 178 * unit};

// A kick's life in Q10 seconds from Damping: 0.4 s dry to 2.4 s ringing.
inline i32 kick_life(i32 damping) { return 410 + ((damping >> 20) * 2048 >> 11); }

// 1024 / life, Q10, from a 16-entry table over the life range: no runtime division.
inline i32 kick_rate(i32 life) {
    static const i32 rates[17] = {2560, 1911, 1524, 1268, 1086, 949, 843, 759, 690, 632, 584, 542, 506, 475, 447, 422, 400};
    const i32 index = clamp((life - 410) * 16 / 2048, 0, 16);
    return rates[index];
}

// 0..1 in Q10 as `value` (0..1023) crosses start..end.
inline i32 fade(i32 value, i32 start, i32 end) {
    return clamp((value - start) * unit / (end - start), 0, unit);
}

inline u32 choice4(i32 control) {
    const u32 c = static_cast<u32>(clamp(control, 0, opxy::q31::maximum)) >> 16;
    const u32 choice = c * 4 >> 15;
    return choice > 3 ? 3 : choice;
}

// Places a petal at the body's centre, turned by tenths of a degree, or hides it.
template <class F>
inline void petal(F& frame, u32 node, i32 tenths, i32 shade) {
    if (shade <= 0) { hide(frame, node); return; }
    place(frame, node, kCentreX, kCentreY, unit, static_cast<u32>(shade > 15 ? 15 : shade), stroke);
    turn(frame, node, rotation(tenths));
}
}

inline void display_reset(DisplayState& state) {
    state.initialized = state.track = state.previous_event = 0;
    state.age = kExpired;
    for (u32 i = 0; i < kRipples; ++i) state.ripple_age[i] = kExpired;
    state.next_ripple = 0;
    state.angle = 0;
    state.model = state.modes = 0xffffffffu;
    state.halo_age[0] = state.halo_age[1] = kHaloLife;
    state.header.reset();
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    const char* const titles[8] = {"Struct", "Bright", "Damping", "Position", "Model", "Strike", "Modes", "Strike"};
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    static const char* const kModelNames[4] = {"modal", "string", "sympath", "chords"};
    static const char* const kModeNames[4] = {"bare", "lean", "full", "lush"};
    const u32 model = choice4(input.slots[4]);
    const u32 modes = choice4(input.slots[6]);
    opxy::draw::copy(values[4], header_capacity, kModelNames[model]);
    opxy::draw::copy(values[6], header_capacity, kModeNames[modes]);
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 2, 6}};
    page_header(state.header, input, frame, titles, values, 0, slots);

    const i32 structure = clamp(input.slots[0], 0, opxy::q31::maximum);
    const i32 brightness = clamp(input.slots[1], 0, opxy::q31::maximum);
    const i32 damping = clamp(input.slots[2], 0, opxy::q31::maximum);
    const i32 position = clamp(input.slots[3], 0, opxy::q31::maximum);
    const i32 strike = clamp(input.slots[5], 0, opxy::q31::maximum);

    // Time: a note resets the kick and starts a ripple; a changed choice lights a halo.
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
    for (u32 i = 0; i < 2; ++i) state.halo_age[i] = clamp(state.halo_age[i] + elapsed, 0, kHaloLife);
    if (!fresh && state.model != model) state.halo_age[0] = 0;
    if (!fresh && state.modes != modes) state.halo_age[1] = 0;
    state.initialized = 1;
    state.track = input.track;
    state.previous_event = input.note_serial;
    state.model = model;
    state.modes = modes;

    // The kick: full at the note, gone after Damping's life; it drives the turn and the glow.
    const i32 life = kick_life(damping);
    const i32 rate = kick_rate(life);
    const i32 flash = clamp(unit - (state.age * rate >> 10), 0, unit);  // Q10, 1 at the note
    if (!input.reduced_motion) {
        const i32 speed = kRestSpeed + (kKickSpeed * flash >> 10);
        state.angle = (state.angle + (speed * elapsed >> 10)) % 3600;
    }
    const i32 bright = 2 + (brightness >> 27);                           // 2..17: Bright lights the inside
    const i32 glow = flash * 5 >> 10;                                    // 0..5 added by a note

    // The body: its circle swells a little with a note; the inside is rings for
    // the modal body and strings for the string bodies, lit by Bright.
    const i32 swell = unit + (flash * 40 >> 10);
    place(frame, kBodyNode, kCentreX, kCentreY, swell, static_cast<u32>(clamp(8 + glow, 1, 15)), stroke);
    place(frame, kCentreNode, kCentreX, kCentreY, unit, static_cast<u32>(clamp(9 + glow, 1, 15)), fill_and_stroke);
    const i32 inside = clamp(bright + glow, 1, 15);
    for (u32 i = 0; i < kInnerRings; ++i) {
        if (model != 0) { hide(frame, kFirstInner + i); continue; }
        place(frame, kFirstInner + i, kCentreX, kCentreY, swell, static_cast<u32>(inside), stroke);
    }
    for (u32 i = 0; i < kStrings4; ++i) {
        const u32 node = kFirstString + i;
        if (model == 0 || (model == 1 && i > 0)) { hide(frame, node); continue; }
        const i32 y = model == 1 ? 0 : (static_cast<i32>(i) * 12 - 18) * unit;
        place(frame, node, kCentreX, kCentreY + y, model == 1 ? unit + unit / 10 : unit, static_cast<u32>(inside), stroke);
    }

    // The pluck point: Position runs along the body's diameter from the left
    // edge (0) to the right (1), through the centre at 0.5. Strike fattens it on the modal body.
    const i32 strike_x = kCentreX - kBodyRadius + ((position >> 16) * 84 >> 5);
    const i32 strike_scale = unit + (model == 0 ? (strike >> 21) * 2 : 0) + flash;
    place(frame, kStrikeNode, strike_x, kCentreY, clamp(strike_scale, unit / 2, 2 * unit),
          static_cast<u32>(8 + (flash * 7 >> 10)), fill_and_stroke);

    // Ripples: circles growing out of the pluck point and fading over the kick's life.
    for (u32 i = 0; i < kRipples; ++i) {
        const u32 node = kFirstRipple + i;
        const i32 progress = state.ripple_age[i] * rate >> 10;  // Q10, 1 at the end of life
        if (input.reduced_motion || progress <= 0 || progress >= unit) { hide(frame, node); continue; }
        const i32 radius = 4 * unit + (progress * 38);           // 4..42 px, to the body's edge
        const i32 shade = clamp(((unit - progress) * (4 + bright)) >> 10, 0, 15);
        if (!shade) { hide(frame, node); continue; }
        place(frame, node, strike_x, kCentreY, clamp(radius / kAuthoredRipple, unit / 8, 2 * unit), static_cast<u32>(shade), stroke);
    }

    // The mandala, lifted from the Rings panel: Structure brings its detail in,
    // the paisleys' outlines first, then their inner teardrops, then the eyes,
    // then the wavy rings, and last the circled dots turning the other way. All
    // share the turn; a note brightens them.
    const i32 detail = structure >> 21;                                  // 0..1023
    const i32 level = 6 + (flash * 6 >> 10);                             // 6..12
    const i32 inner_fade = fade(detail, 40, 300);
    const i32 eye_fade = fade(detail, 280, 560);
    const i32 ring_fade = fade(detail, 520, 780);
    const i32 dot_fade = fade(detail, 740, 980);
    for (u32 i = 0; i < kPaisleys; ++i) {
        const i32 tenths = state.angle + static_cast<i32>(i) * 300;
        petal(frame, kFirstOutline + i, tenths, level);
        petal(frame, kFirstInnerDrop + i, tenths, (level - 1) * inner_fade >> 10);
        const i32 eye = (level - 2) * eye_fade >> 10;
        petal(frame, kFirstEye + i, tenths, eye);
        petal(frame, kFirstPupil + i, tenths, eye);
        petal(frame, kFirstDrop + i, tenths, eye);
    }
    petal(frame, kRingInner, state.angle, (level - 2) * ring_fade >> 10);
    petal(frame, kRingOuter, state.angle, (level - 3) * ring_fade >> 10);
    const i32 counter = 3600 - (state.angle * 3 >> 2) + 150;
    for (u32 i = 0; i < kPaisleys; ++i) {
        const i32 tenths = counter + static_cast<i32>(i) * 300;
        petal(frame, kFirstDotRing + i, tenths, (level - 2) * dot_fade >> 10);
        petal(frame, kFirstDot + i, tenths, (level - 2) * dot_fade >> 10);
    }

    // The LEDs: the chosen one lit, a halo on it while the change is fresh.
    for (u32 column = 0; column < 2; ++column) {
        const u32 chosen = column == 0 ? model : modes;
        const u32 first = column == 0 ? kFirstModelLed : kFirstModesLed;
        for (u32 i = 0; i < 4; ++i)
            place(frame, first + i, kLedX[column], kLedY[i], unit, i == chosen ? 15u : 4u, stroke);
        const i32 halo = clamp(unit - (state.halo_age[column] * unit / kHaloLife), 0, unit);
        const i32 shade = halo * 9 >> 10;
        if (shade <= 0) hide(frame, kHaloNode + column);
        else place(frame, kHaloNode + column, kLedX[column], kLedY[chosen], unit, static_cast<u32>(shade), stroke);
    }
}
}
