// The screen: the shared two-page header; the model and modes LEDs either side,
// as on the Rings panel, lit by the choice and haloed when it changes; and in
// the middle the body, a circle with a dot at its centre, inside a mandala of
// petals that turns slowly, kicks on every note and eases back as the note
// fades. Structure adds petals, Bright lights the body's inside,
// Damping sets how long the kick lasts, Position shapes the body's inside.
// Each model has its own mandala: Rings'
// Frequency paisleys for the modal body, a lotus for the string body, circles
// of leaf veins for the sympathetic, a layered flower for the chords. Inside
// the body each model has its own life, drawn fresh every frame as mutable
// outlines: rings for the modal body, a meter for the string body, S-waves
// that bend for the sympathetic, lines that vibrate for the chords. Position
// adds rings, bars and strings from the centre out.
#include "display.h"
#include "core.h"
#include "series.h"

namespace mi_rings {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
const i32 kCentreX = 240 * unit, kCentreY = 130 * unit;
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

// The mutable outlines: each is a thin closed ribbon of exactly kPathPoints
// points, in Q10 pixels about the body's centre, one edge out and the other
// back a fraction of a pixel away so it encloses area (the OP-XY streaks
// otherwise). The body's radius is 40 px, so the ends sit at 39.4 px.
const i32 kEdge = 410;                                            // 0.4 px: the ribbon's half width
const i32 kRibbon = 819;                                          // 0.8 px between the two edges
struct Pen { Frame::path_type& path; u32 n; };
inline void put(Pen& pen, i32 x, i32 y) {
    if (pen.n < kPathPoints) pen.path.points[pen.n] = {x, y};
    ++pen.n;
}
// The string body's meter: seven bars on a baseline from edge to edge, each
// `height[i]` above and below it (Q10 px, at least kEdge + a little). Out
// along the top edge drawing the bars' upper halves, back along the bottom
// drawing their lower halves: 64 points exactly. Outlines are never filled,
// so the bars are 1.6 px wide: the strokes of their two sides overlap.
const u32 kMeterBars = 7;
const i32 kMeterX[kMeterBars] = {-30, -20, -10, 0, 10, 20, 30};
const i32 kMeterReach[kMeterBars] = {20, 26, 30, 32, 30, 26, 20};   // px: the tallest bar inside the circle
inline void meter(Frame::path_type& path, const i32 (&height)[kMeterBars]) {
    Pen pen = {path, 0};
    const i32 end = 39 * unit, half = 819;
    put(pen, -end, -kEdge);
    put(pen, -36 * unit, -kEdge);
    for (u32 i = 0; i < kMeterBars; ++i) {
        const i32 x = kMeterX[i] * unit;
        put(pen, x - half, -kEdge); put(pen, x - half, -height[i]); put(pen, x + half, -height[i]); put(pen, x + half, -kEdge);
    }
    put(pen, 36 * unit, -kEdge); put(pen, end, -kEdge); put(pen, end, kEdge); put(pen, 36 * unit, kEdge);
    for (u32 k = kMeterBars; k-- > 0;) {
        const i32 x = kMeterX[k] * unit;
        put(pen, x + half, kEdge); put(pen, x + half, height[k]); put(pen, x - half, height[k]); put(pen, x - half, kEdge);
    }
    put(pen, -36 * unit, kEdge);
    put(pen, -end, kEdge);
}
// A vertical S-wave of a turn and a half down column x0 (px) from the body's
// edge to its edge, bent `amplitude` (Q10 px) sideways: 32 points down, 32 back.
inline void s_wave(Frame::path_type& path, i32 x0, i32 half, i32 amplitude) {
    Pen pen = {path, 0};
    const u32 steps = kPathPoints / 2 - 1;
    i32 xs[kPathPoints / 2];
    for (u32 i = 0; i <= steps; ++i)
        xs[i] = x0 * unit + (amplitude * sine(static_cast<i32>(i) * 5400 / static_cast<i32>(steps) % 3600) >> 10);
    for (u32 i = 0; i <= steps; ++i)
        put(pen, xs[i], -half + (2 * half * static_cast<i32>(i) / static_cast<i32>(steps)));
    for (u32 i = steps + 1; i-- > 0;)
        put(pen, xs[i] + kRibbon, -half + (2 * half * static_cast<i32>(i) / static_cast<i32>(steps)));
}
// A string across the body at row y0 (px), from edge to edge, standing in its
// `mode`th harmonic with the given amplitude (Q10 px): 32 points out, 32 back.
inline void string_line(Frame::path_type& path, i32 y0, i32 half, i32 mode, i32 amplitude) {
    Pen pen = {path, 0};
    const u32 steps = kPathPoints / 2 - 1;
    i32 ys[kPathPoints / 2];
    for (u32 i = 0; i <= steps; ++i)
        ys[i] = y0 * unit + (amplitude * sine(static_cast<i32>(i) * 1800 * mode / static_cast<i32>(steps)) >> 10);
    for (u32 i = 0; i <= steps; ++i)
        put(pen, -half + (2 * half * static_cast<i32>(i) / static_cast<i32>(steps)), ys[i]);
    for (u32 i = steps + 1; i-- > 0;)
        put(pen, -half + (2 * half * static_cast<i32>(i) / static_cast<i32>(steps)), ys[i] + kRibbon);
}
// Two strings at rows -y0 and y0 (px) as one outline: the upper string out,
// an arc down the body's edge on one side (under the body circle's stroke, so
// it does not show), the lower string back, and the same the other way a
// fraction of a pixel inside. 12 points along each string, 8 along each arc.
// `x_end` is the strings' half length (Q10), `theta` the angle of their ends
// in tenths of a degree, `side` 1 for the arc on the right, -1 on the left.
inline void string_pair(Frame::path_type& path, i32 y0, i32 x_end, i32 theta, i32 side,
                        i32 mode_up, i32 amp_up, i32 mode_down, i32 amp_down) {
    Pen pen = {path, 0};
    const i32 along = 12, arc = 8, radius = 40 * unit;
    i32 xs[along], up[along], down[along];
    for (i32 i = 0; i < along; ++i) {
        xs[i] = side * (-x_end + 2 * x_end * i / (along - 1));
        up[i] = -y0 * unit + (amp_up * sine(i * 1800 * mode_up / (along - 1)) >> 10);
        down[i] = y0 * unit + (amp_down * sine(i * 1800 * mode_down / (along - 1)) >> 10);
    }
    for (i32 i = 0; i < along; ++i) put(pen, xs[i], up[i] - kEdge);
    for (i32 j = 1; j <= arc; ++j) {
        const i32 angle = -theta + 2 * theta * j / (arc + 1);
        put(pen, side * (radius * sine(angle + 900) >> 10), radius * sine(angle) >> 10);
    }
    for (i32 i = along; i-- > 0;) put(pen, xs[i], down[i] + kEdge);
    for (i32 i = 0; i < along; ++i) put(pen, xs[i], down[i] - kEdge);
    for (i32 j = arc; j >= 1; --j) {
        const i32 angle = -theta + 2 * theta * j / (arc + 1);
        put(pen, side * ((radius - kEdge) * sine(angle + 900) >> 10), (radius - kEdge) * sine(angle) >> 10);
    }
    for (i32 i = along; i-- > 0;) put(pen, xs[i], up[i] + kEdge);
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
    for (u32 i = 0; i < kWaves; ++i) state.wave_age[i] = kExpired;
    state.next_wave = 0;
    state.angle = 0;
    state.model = state.modes = 0xffffffffu;
    state.halo_age[0] = state.halo_age[1] = kHaloLife;
    state.serial = 1;
    state.header.reset();
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    const char* const titles[8] = {"Struct", "Bright", "Damping", "Position", "Model", "Strike", "Modes", "Strike"};
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    static const char* const kModelNames[4] = {"mdal", "strn", "symp", "chrd"};
    static const char* const kModeNames[4] = {"bare", "lean", "full", "lush"};
    const u32 model = choice4(input.slots[4]);
    const u32 modes = choice4(input.slots[6]);
    opxy::draw::copy(values[4], header_capacity, kModelNames[model]);
    opxy::draw::copy(values[6], header_capacity, kModeNames[modes]);
    const opxy::u8 slots[2][4] = {{0, 1, 2, 3}, {4, 5, 3, 6}};     // page 2: Model, Strike, Position shared, Modes
    page_header(state.header, input, frame, titles, values, kHeaderNode, slots);

    const i32 structure = clamp(input.slots[0], 0, opxy::q31::maximum);
    const i32 brightness = clamp(input.slots[1], 0, opxy::q31::maximum);
    const i32 damping = clamp(input.slots[2], 0, opxy::q31::maximum);
    const i32 position = clamp(input.slots[3], 0, opxy::q31::maximum);
    const i32 strike = clamp(input.slots[5], 0, opxy::q31::maximum);
    const i32 boost = unit + (strike >> 22);                             // Q10, 1..1.5: Strike exaggerates the insides' motion

    // Time: a note resets the kick and starts a light wave; a changed choice lights a halo.
    const i32 elapsed = clamp(input.elapsed, 0, kExpired);
    const bool fresh = !state.initialized || state.track != input.track;
    if (fresh || input.reduced_motion) {
        state.age = kExpired;
        for (u32 i = 0; i < kWaves; ++i) state.wave_age[i] = kExpired;
    } else {
        state.age = clamp(state.age + elapsed, 0, kExpired);
        for (u32 i = 0; i < kWaves; ++i) state.wave_age[i] = clamp(state.wave_age[i] + elapsed, 0, kExpired);
        if (input.track < 8 && input.note_serial != state.previous_event) {
            state.age = 0;
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
    place(frame, kBodyNode, kCentreX, kCentreY, swell, static_cast<u32>(clamp(8 + glow, 1, 15)), stroke);
    place(frame, kCentreNode, kCentreX, kCentreY, unit, static_cast<u32>(clamp(9 + glow, 1, 15)), fill_and_stroke);
    const i32 inside = clamp(bright + glow, 1, 15);
    const i32 reach = position >> 21;                                    // 0..1023: how far Position goes
    // The modal body: rings from the centre out, as many as Position asks
    // for, none at 0 and all at full, the newest fading in. Authored at the
    // body's radius, so ring i sits at (i + 1) / (kInnerRings + 1) of it.
    for (u32 i = 0; i < kInnerRings; ++i) {
        const i32 shown = clamp(reach * static_cast<i32>(kInnerRings) - static_cast<i32>(i) * unit, 0, unit);
        const i32 shade = inside * shown >> 10;
        if (model != kModal || shade <= 0) { hide(frame, kFirstInner + i); continue; }
        place(frame, kFirstInner + i, kCentreX, kCentreY, swell * static_cast<i32>(i + 1) / static_cast<i32>(kInnerRings + 1), static_cast<u32>(shade), stroke);
    }
    // The mutable outlines, drawn fresh every frame for the model and placed at
    // the body's centre at its swell: the key changes with them so the
    // renderers reapply them. The modal body uses none; the others' sit flat.
    state.serial += 1;
    if (state.serial < 2) state.serial = 2;
    i32 path_shade[kPaths] = {0, 0, 0, 0};                             // 0: that outline is not drawn
    if (model == kString) {
        // The meter: a line at rest, its seven bars rising to a loose spectrum
        // on a note, each wavering at its own rate, and settling as it fades.
        // Position brings the bars in from the centre: one at 0, all seven at full.
        static const i32 kLevel[kMeterBars] = {600, 820, 940, 1024, 900, 760, 560};   // Q10 of the bar's reach
        static const i32 kRate[kMeterBars] = {12, 10, 16, 8, 18, 11, 14};     // tenths per Q10 second: 2..5 Hz, within the screen's rate
        static const i32 kPhase[kMeterBars] = {0, 500, 1000, 1500, 2000, 2500, 3000};
        i32 height[kMeterBars];
        for (u32 i = 0; i < kMeterBars; ++i) {
            const i32 waver = unit - (unit - sine((state.age * kRate[i] + kPhase[i]) % 3600)) * 350 / 2048;  // 0.65..1
            const i32 pair = i < 3 ? 3 - static_cast<i32>(i) : static_cast<i32>(i) - 3;                  // 0 centre, 3 outermost
            const i32 shown = clamp(reach * 3 - (pair - 1) * unit, 0, unit);                                  // Q10
            const i32 level = clamp((((kLevel[i] * flash >> 10) * boost >> 10) * waver >> 10) * shown >> 10, 0, unit);  // Q10
            height[i] = kEdge + 20 + (kMeterReach[i] * level);                                           // Q10 px
        }
        meter(frame.paths[0], height);
        path_shade[0] = inside;
    } else if (model == kSympathetic) {
        // Three S-waves from edge to edge, bent a little at rest and more while a note rings.
        static const i32 kColumn[3] = {-13, 0, 13}, kHalf[3] = {37 * unit + 205, 39 * unit + 410, 37 * unit + 205};
        const i32 amplitude = 3 * unit + ((5 * unit * flash >> 10) * boost >> 10);
        for (u32 i = 0; i < 3; ++i) { s_wave(frame.paths[i], kColumn[i], kHalf[i], amplitude); path_shade[i] = inside; }
    } else if (model == kChords) {
        // Strings across the body, standing still at rest and shaking in their
        // harmonics while a note rings, their ends fixed at the edge: one at the
        // centre, then pairs either side of it that Position brings in from the
        // centre out, one at 0 and seven at full.
        static const i32 kRow[3] = {9, 18, 27};
        static const i32 kEnd[3] = {38 * unit + 993, 35 * unit + 737, 29 * unit + 522};   // sqrt(40^2 - row^2)
        static const i32 kTheta[3] = {130, 267, 425};                                     // asin(row / 40), tenths
        static const i32 kMode[7] = {3, 2, 1, 1, 3, 2, 2}, kRate[7] = {14, 11, 10, 12, 10, 15, 9};   // 2.5..4 Hz
        i32 amplitude[7];
        for (u32 i = 0; i < 7; ++i) {
            const i32 shake = sine((state.age * kRate[i] + static_cast<i32>(i) * 700) % 3600);
            amplitude[i] = (((3 * unit + 205) * flash >> 10) * boost >> 10) * shake >> 10;
        }
        string_line(frame.paths[0], 0, 39 * unit + 410, kMode[0], amplitude[0]);
        path_shade[0] = inside;
        for (u32 k = 0; k < 3; ++k) {
            const i32 shown = clamp(reach * 3 - static_cast<i32>(k) * unit, 0, unit);
            if (shown <= 0) continue;
            string_pair(frame.paths[k + 1], kRow[k], kEnd[k], kTheta[k], (k & 1) ? -1 : 1,
                        kMode[1 + 2 * k], amplitude[1 + 2 * k], kMode[2 + 2 * k], amplitude[2 + 2 * k]);
            path_shade[k + 1] = inside * shown >> 10;
        }
    }
    for (u32 i = 0; i < kPaths; ++i) {
        if (path_shade[i] > 0) {
            frame.paths[i].key = state.serial;
            place(frame, i, kCentreX, kCentreY, swell, static_cast<u32>(path_shade[i]), stroke);
        } else {
            if (frame.paths[i].key != 1) { string_line(frame.paths[i], 0, 39 * unit, 1, 0); frame.paths[i].key = 1; }
            hide(frame, i);
        }
    }

    // The mandala: the model's own, lifted from Mutable's panels, its pieces in
    // layers from the smallest out. Structure brings the layers in in that
    // order, and a note lights them in that order too, a wave from the centre
    // out over the kick's life. Each recent note has its own wave, so a new
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
                const i32 turned = (group.fill & 2) ? -state.angle : state.angle;   // some pieces turn the other way
                petal(frame, group.first + i, turned + static_cast<i32>(i) * step, layer[group.layer],
                      (group.fill & 1) ? fill_and_stroke : stroke);
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
