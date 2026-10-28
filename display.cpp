// The screen: the shared two-page header; the model and modes LEDs either side,
// as on the Rings panel, lit by the choice and haloed when it changes; and in
// the middle the body, a circle with a dot at its centre, inside a mandala of
// petals that turns slowly, kicks on every note and eases back as the note
// fades. Structure adds petals, Bright lights the body's inside,
// Damping sets how long the kick lasts, Position places the pluck point and the
// ripples that spread from it. Each model has its own mandala, lifted from
// Mutable's panels: Rings' Frequency paisleys for the modal body, Elements'
// Mallet and Flow knobs for the string and sympathetic bodies, Rings' Structure
// knob for the chords.
#include "display.h"
#include "core.h"
#include "series.h"

namespace mi_rings {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
const i32 kCentreX = 240 * unit, kCentreY = 130 * unit;
const i32 kBodyRadius = 40 * unit;
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
inline void petal(F& frame, u32 node, i32 tenths, i32 shade, Paint paint = stroke) {
    if (shade <= 0) { hide(frame, node); return; }
    place(frame, node, kCentreX, kCentreY, unit, static_cast<u32>(shade > 15 ? 15 : shade), paint);
    turn(frame, node, rotation(tenths));
}
}

inline void display_reset(DisplayState& state) {
    state.initialized = state.track = state.previous_event = 0;
    state.age = kExpired;
    for (u32 i = 0; i < kRipples; ++i) state.ripple_age[i] = kExpired;
    state.next_ripple = 0;
    for (u32 i = 0; i < kWaves; ++i) state.wave_age[i] = kExpired;
    state.next_wave = 0;
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
        for (u32 i = 0; i < kWaves; ++i) state.wave_age[i] = kExpired;
    } else {
        state.age = clamp(state.age + elapsed, 0, kExpired);
        for (u32 i = 0; i < kRipples; ++i) state.ripple_age[i] = clamp(state.ripple_age[i] + elapsed, 0, kExpired);
        for (u32 i = 0; i < kWaves; ++i) state.wave_age[i] = clamp(state.wave_age[i] + elapsed, 0, kExpired);
        if (input.track < 8 && input.note_serial != state.previous_event) {
            state.age = 0;
            state.ripple_age[state.next_ripple] = 0;
            state.next_ripple = (state.next_ripple + 1) % kRipples;
            state.wave_age[state.next_wave] = 0;
            state.next_wave = (state.next_wave + 1) % kWaves;
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

    // The body: its circle, shrunk to sit inside the model's mandala, swells a
    // little with a note. Inside, lit by Bright: rings for the modal body,
    // meter bars for the string body, waves for the sympathetic, lines for
    // the chords, each stirred by a note and kept within the circle.
    const Mandala& mandala = kMandalas[model];
    const i32 body = mandala.body_scale;                                 // Q10
    const i32 swell = (unit + (flash * 40 >> 10)) * body >> 10;
    const i32 body_radius = kBodyRadius * body >> 10;
    place(frame, kBodyNode, kCentreX, kCentreY, swell, static_cast<u32>(clamp(8 + glow, 1, 15)), stroke);
    place(frame, kCentreNode, kCentreX, kCentreY, unit, static_cast<u32>(clamp(9 + glow, 1, 15)), fill_and_stroke);
    const i32 inside = clamp(bright + glow, 1, 15);
    const i32 tremor = sine((state.age * 30) % 3600);                    // Q10, about 8 Hz
    const i32 quiver = sine((state.age * 41 + 900) % 3600);
    for (u32 i = 0; i < kInnerRings; ++i) {
        if (model != kModal) { hide(frame, kFirstInner + i); continue; }
        place(frame, kFirstInner + i, kCentreX, kCentreY, swell, static_cast<u32>(inside), stroke);
    }
    // The string body: five meter bars, short at rest, that leap to a loose
    // spectrum on a note, each wavering at its own rate, and settle as it fades.
    // The authored bar is 14 px tall, so scale 0.3 is a tick and 2 a full bar.
    static const i32 kBarLevel[kBars] = {1500, 2048, 1750, 2048, 1500};   // Q10 scales when struck
    static const i32 kBarRate[kBars] = {37, 29, 47, 23, 53};
    static const i32 kBarPhase[kBars] = {0, 700, 1400, 2100, 2800};
    for (u32 i = 0; i < kBars; ++i) {
        const u32 node = kFirstBar + i;
        if (model != kString) { hide(frame, node); continue; }
        const i32 waver = unit - (unit - sine((state.age * kBarRate[i] + kBarPhase[i]) % 3600)) * 300 / 2048;  // 0.7..1
        const i32 level = clamp(307 + ((kBarLevel[i] - 307) * flash >> 10) * waver / unit, 307, 2 * unit);
        const i32 x = (static_cast<i32>(i) * 14 - 28) * unit;
        place(frame, node, kCentreX + (x * body >> 10), kCentreY, level * body >> 10, static_cast<u32>(inside), fill_and_stroke);
    }
    // The sympathetic body: three S-waves from edge to edge that bend on a
    // note: each has a calm and a bent outline, cross-faded by the kick.
    for (u32 i = 0; i < kWaves3; ++i) {
        const i32 x = kWaveX[i] * swell;
        const i32 calm = inside * (unit - flash) >> 10, bent = inside * flash >> 10;
        if (model != kSympathetic || calm <= 0) hide(frame, kFirstWave + i);
        else place(frame, kFirstWave + i, kCentreX + x, kCentreY, swell, static_cast<u32>(calm), stroke);
        if (model != kSympathetic || bent <= 0) hide(frame, kFirstWave + kWaves3 + i);
        else place(frame, kFirstWave + kWaves3 + i, kCentreX + x, kCentreY, swell, static_cast<u32>(bent), stroke);
    }
    // The chords body: three lines across the body that quiver on a note.
    for (u32 i = 0; i < kChords3; ++i) {
        const u32 node = kFirstChord + i;
        if (model != kChords) { hide(frame, node); continue; }
        const i32 side = (i & 1) ? -1 : 1;
        const i32 wave = (i == 1 ? quiver : tremor) * flash >> 10;      // Q10, fading with the kick
        const i32 y = kChordY[i] * swell + side * wave * 3 / 2;
        place(frame, node, kCentreX, kCentreY + y, swell, static_cast<u32>(inside), stroke);
        turn(frame, node, rotation(side * wave * 15 >> 10));
    }

    // The pluck point: Position runs along the body's diameter from the left
    // edge (0) to the right (1), through the centre at 0.5. Strike fattens it on the modal body.
    const i32 strike_x = kCentreX - body_radius + ((((position >> 16) * 80 >> 5) * body) >> 10);
    const i32 strike_scale = unit + (model == 0 ? (strike >> 21) * 2 : 0) + flash;
    place(frame, kStrikeNode, strike_x, kCentreY, clamp(strike_scale, unit / 2, 2 * unit),
          static_cast<u32>(8 + (flash * 7 >> 10)), fill_and_stroke);

    // Ripples: circles growing out of the pluck point and fading over the kick's life.
    for (u32 i = 0; i < kRipples; ++i) {
        const u32 node = kFirstRipple + i;
        const i32 progress = state.ripple_age[i] * rate >> 10;  // Q10, 1 at the end of life
        if (input.reduced_motion || progress <= 0 || progress >= unit) { hide(frame, node); continue; }
        const i32 radius = (4 * unit + (progress * 36)) * body >> 10;   // 4..40 px, to the body's edge
        const i32 shade = clamp(((unit - progress) * (4 + bright)) >> 10, 0, 15);
        if (!shade) { hide(frame, node); continue; }
        place(frame, node, strike_x, kCentreY, clamp(radius / kAuthoredRipple, unit / 8, 2 * unit), static_cast<u32>(shade), stroke);
    }

    // The mandala: the model's own, lifted from Mutable's panels, its pieces in
    // layers from the smallest out. Structure brings the layers in in that
    // order, and a note lights them in that order too, a ripple from the centre
    // out over the kick's life. Each recent note has its own ripple, so a new
    // note never cancels one in flight. The other models' pieces stay hidden.
    const i32 detail = structure >> 21;                                  // 0..1023
    i32 layer[kLayers];
    for (u32 k = 0; k < kLayers; ++k) {
        const i32 start = static_cast<i32>(k) * 150 - 130;              // the first layer always
        const i32 shown = k == 0 ? unit : fade(detail, start, start + 160);
        i32 pulse = 0;
        for (u32 w = 0; w < kWaves; ++w) {
            const i32 progress = clamp(state.wave_age[w] * rate >> 10, 0, unit);  // 0..1 over the kick's life
            if (progress >= unit) continue;
            const i32 away = progress - static_cast<i32>(k) * 96;
            const i32 here = clamp(unit - (away < 0 ? -away : away) * 4, 0, unit);
            if (here > pulse) pulse = here;
        }
        layer[k] = (6 * shown + 7 * pulse) >> 10;
    }
    for (u32 m = 0; m < kModels; ++m) {
        const Mandala& set = kMandalas[m];
        for (u32 g = 0; g < kMandalaGroups; ++g) {
            const MandalaGroup& group = set.groups[g];
            if (!group.count) break;
            const i32 step = group.count > 1 ? 3600 / static_cast<i32>(group.count) : 0;
            for (u32 i = 0; i < group.count; ++i) {
                if (m != model) { hide(frame, group.first + i); continue; }
                petal(frame, group.first + i, state.angle + static_cast<i32>(i) * step, layer[group.layer],
                      group.fill ? fill_and_stroke : stroke);
            }
        }
    }

    // The LEDs: the chosen one lit over the artwork's rings, a halo on it while the change is fresh.
    for (u32 column = 0; column < 2; ++column) {
        const u32 chosen = column == 0 ? model : modes;
        place(frame, column == 0 ? kModelLed : kModesLed, kLedX[column], kLedY[chosen], unit, 15u, stroke);
        const i32 halo = clamp(unit - (state.halo_age[column] * unit / kHaloLife), 0, unit);
        const i32 shade = halo * 9 >> 10;
        if (shade <= 0) hide(frame, kHaloNode + column);
        else place(frame, kHaloNode + column, kLedX[column], kLedY[chosen], unit, static_cast<u32>(shade), stroke);
    }
}
}
