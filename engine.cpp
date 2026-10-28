// mi_rings: Mutable Instruments Rings' resonator models (rings/dsp, MIT,
// Emilie Gillet), rewritten in fixed point for the OP-XY. Modal: a note
// strikes a bank of up to 24 two-pole resonators tuned to a partial series that
// Structure bends from compressed, through harmonic, to bell-like; Bright sets
// the strike's colour and how fast the high partials fade; Damping the decay;
// Position the strike point. The string models are in strings.h.
#include "core.h"
#include <opxy/q31.hpp>

namespace mi_rings {
using opxy::i32;
using opxy::u32;
using opxy::q31::multiply;

namespace {
const i32 kOne = 0x7fffffff;
const i32 kHalfPiLessOne = 1225724076;  // pi/2 - 1
const i32 kPiOver4 = 1686629713;        // pi/4
const i32 kThird = 715827883;           // 1/3
const i32 kTwoFifteenths = 286331153;   // 2/15
const i32 kExciterR = 1431655765;       // 1/Q of the strike filter, Q 1.5
const i32 kFifth = 429496730;           // 0.2
const i32 kTenth = 214748365;           // 0.1
const i32 kPoint85 = 1825361101;        // 0.85
const i32 kPoint4 = 858993459;          // 0.4: the limiter's 1.4 pre-gain
const i32 kRelease = 2748779;           // the limiter's 0.00002 per sample, over a block
const i32 kOutput = 1202590843;         // 0.56: Rings' 1.4 x 0.8, halved for the final shift
const i32 kOutputUnity = 858993459;     // 0.4: the same for the string models' gain of 1
const i32 kFrequencyLimit = 2055208;    // 0.49 cycles per sample, Q22: the band's top, as in Rings
const i32 kCutoffLimit = 515396075;     // 0.24 cycles per sample, Q31: the strike filter's reach
const i32 kHeavy = 1 << 17;             // a loss per sample from 2^-14: applied every sample
const i32 kIdle = 1 << 4;               // a mode state below this, 2^-20 of unity, is silent
// 2^(-steps/2): a noise burst of 2^steps samples keeps the pulse's energy.
const i32 kBurstGain[12] = {2147483647, 1518500250, 1073741824, 759250125, 536870912, 379625062,
                            268435456, 189812531, 134217728, 94906266, 67108864, 47453133};

inline i32 clamp(i32 value, i32 low, i32 high) { return value < low ? low : value > high ? high : value; }

// x (2 - x) for a control, as 1 - (1 - x)^2 so a full control cannot overflow.
inline i32 bend(i32 x) { const i32 rest = kOne - x; return kOne - multiply(rest, rest); }

// value * 2^n, saturating.
inline i32 shift(i32 value, int n) {
    if (n <= 0) return n <= -31 ? 0 : value >> -n;
    if (n >= 31) return kOne;
    return value > (kOne >> n) ? kOne : value << n;
}

// 1/den for a positive 32-bit word: den << e lies in [2^31, 2^32) and
// r = 2^62 / (den << e), so a / den is multiply(a, r) * 2^(e - 31); divide folds
// the formats of a, den and the result into scale = B - A + S - 31.
struct Reciprocal { i32 r; int e; };
inline Reciprocal reciprocal(u32 den) {
    Reciprocal out = {0, 0};
    u32 m = den;
    if (!(m & 0xffff0000u)) { m <<= 16; out.e += 16; }
    if (!(m & 0xff000000u)) { m <<= 8; out.e += 8; }
    if (!(m & 0xf0000000u)) { m <<= 4; out.e += 4; }
    if (!(m & 0xc0000000u)) { m <<= 2; out.e += 2; }
    if (!(m & 0x80000000u)) { m <<= 1; out.e += 1; }
    const i32 x = static_cast<i32>(m >> 1);                            // Q31 in [0.5, 1)
    u32 y = 3129096000u - static_cast<u32>(x);                         // 2.9142 - 2x, Q30
    for (int i = 0; i < 3; ++i) {                                      // Newton: y (2 - x y)
        const i32 xy = multiply(x, static_cast<i32>(y));               // Q30
        const i32 rest = static_cast<i32>(2147483648u - static_cast<u32>(xy));
        y = static_cast<u32>(multiply(static_cast<i32>(y), rest)) << 1;
        if (y > 0x7fffffffu) y = 0x7fffffffu;
    }
    out.r = static_cast<i32>(y);
    return out;
}
// (a / den) * 2^scale for a quotient whose formats need 2^scale beyond the division.
inline i32 divide(i32 a, Reciprocal den, int scale) { return shift(multiply(a, den.r), den.e + scale); }

// value * 2^(x), x in Q28, saturating.
inline i32 exp2_scale(i32 value, i32 x) {
    // floor(x / 2^28) without overflow: -x + 2^28 overflows for x below
    // -(2^31 - 2^28), which Damping's loss exponent reaches above 0.646.
    const i32 n = x >= 0 ? x >> 28 : -(((-(x + 1)) >> 28) + 1);
    const u32 fraction = static_cast<u32>(x - (n << 28));          // Q28, below 1
    const u32 index = fraction >> 22;
    const i32 within = static_cast<i32>((fraction & 0x3fffffu) << 9);
    const i32 e = kExp2[index] + multiply(kExp2[index + 1] - kExp2[index], within);  // Q30
    return shift(multiply(value, e), n + 1);
}

inline i32 noise(u32& seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return static_cast<i32>(seed);
}

// x - loss * x, carrying the fraction so the loss stays exact at any level.
inline i32 lose(i32 x, i32 loss, u32& carry) {
    const i32 whole = multiply(loss, x);
    carry += (static_cast<u32>(loss) * static_cast<u32>(x)) & 0x7fffffffu;
    const i32 round = static_cast<i32>(carry >> 31);
    carry &= 0x7fffffffu;
    return x - (whole + round);
}

// Tune every mode to the note and the controls.
void tune(Voice& voice, i32 structure, i32 brightness, i32 damping, i32 position, u32 count, i32 ease, u32 pitch) {
    Series series;
    compute_series(structure, position, count, series);
    const i32 f0 = static_cast<i32>(pitch >> 4);     // Q28 cycles per sample
    i32 quality = lookup(kQuality, damping);          // Rings' q, Q8
    // Rings feeds the resonator brightness squared, then narrows its range at
    // low Structure to keep the sum in check.
    const i32 squared = multiply(brightness, brightness);
    i32 attenuation = kOne - structure;
    attenuation = multiply(attenuation, attenuation);
    attenuation = multiply(attenuation, attenuation);
    attenuation = multiply(attenuation, attenuation);
    const i32 bright = multiply(squared, kOne - multiply(attenuation, kFifth));
    i32 q_loss = kOne - multiply(kOne - bend(bright), kPoint85);  // 0.15 + 0.85 bright (2 - bright)
    q_loss += multiply(ease, kOne - q_loss);                       // lush keeps its top partials ringing
    const i32 q_loss_rate = multiply(bend(structure), kTenth);
    i32 harmonic = 0;  // Q28
    // A partial that joins a ringing note starts from the fundamental's state,
    // rolled off by 1/n^2 as the strike's two-pole low-pass would have left
    // it, so a turn up of Size is heard before the next strike. Rings leaves
    // a new mode silent until it is struck, and the top ringing partial is
    // no reference: at Bright 50 it has died within the first tenth of a second.
    i32 seed_x = 0, seed_y = 0;
    if (voice.count > 0 && voice.modes[0].active) {
        seed_x = voice.modes[0].x;
        seed_y = voice.modes[0].y;
    }
    voice.count = 0;
    for (u32 i = 0; i < kMaxModes; ++i) {
        Mode& mode = voice.modes[i];
        if (i >= count) {
            if (mode.active) mode.x = mode.y = 0;
            mode.active = 0;
            continue;
        }
        harmonic += f0;
        const i32 partial = multiply(harmonic, series.stretch[i]);  // Q22
        if (partial >= kFrequencyLimit) {
            // Above the band: silent, and like Rings not counted, but the chain goes on.
            if (mode.active) mode.x = mode.y = 0;
            mode.active = 0;
        } else {
            const i32 f = partial << 9;  // Q31
            mode.rotation = sine_quarter(f << 1);
            mode.drive = f + multiply(f, kHalfPiLessOne);           // (pi/2) f, also pi f in Q30
            const u32 q = 256u + static_cast<u32>(multiply(f, quality));  // 1 + f q, Q8
            const i32 loss = divide(mode.drive, reciprocal(q), -22);  // pi f / Q per sample, Q31
            if (loss >= kHeavy) {
                mode.heavy = 1;
                mode.decay = kOne - loss;
            } else {
                mode.heavy = 0;
                mode.decay = loss << 6;
            }
            mode.amplitude = series.amplitude[i];
            if (!mode.active) {
                mode.carry_x = mode.carry_y = 0;
                const i32 scale = i ? divide(1 << 20, reciprocal((i + 1) * (i + 1)), -20) : kOne;  // 1 / (i + 1)^2, Q31
                mode.x = multiply(seed_x, scale);
                mode.y = multiply(seed_y, scale);
            }
            mode.active = 1;
            voice.count = i + 1;
        }
        q_loss += multiply(q_loss_rate, kOne - q_loss);
        quality = multiply(quality, q_loss);
    }
}

// Arm the strike: Rings' pulse through a low-pass that Bright places from a
// sixteenth of the note to sixteen times it, at Q 1.5.
void strike(Voice& voice, i32 brightness, u32 pitch, i32 strike_control) {
    Exciter& e = voice.exciter;
    const i32 colour = bend(brightness);  // b (2 - b)
    const i32 f0 = static_cast<i32>(pitch >> 1);                                      // Q31
    i32 cutoff = exp2_scale(f0, colour - (1 << 30));                                  // f0 2^((c - 0.5) 8)
    if (cutoff > kCutoffLimit) cutoff = kCutoffLimit;
    if (cutoff < 1024) cutoff = 1024;
    // g = tan(pi fc) by series; t = pi fc in Q30 reads the Q31 cutoff as 2 fc.
    const i32 t = cutoff + multiply(cutoff, kHalfPiLessOne);
    const i32 t2 = multiply(t, t) * 2;
    const i32 t4 = multiply(t2, t2) * 2;
    const i32 poly = multiply(t2, kThird) + multiply(t4, kTwoFifteenths);  // tan(t)/t - 1, Q30
    const i32 g30 = t + multiply(t, poly) * 2;
    e.g = g30 * 2;
    const u32 den = (1u << 29) + (static_cast<u32>(multiply(kExciterR, g30)) >> 1) +
                    static_cast<u32>(multiply(g30, g30));                       // 1 + r g + g^2, Q29
    e.h = divide(1 << 30, reciprocal(den), -1);
    // The pulse's energy as it meets the filter: g A = (pi/4) 2^(2 c^2) tan(pi fc) / (pi fc).
    const i32 c2 = multiply(colour, colour);
    const i32 base = exp2_scale(kPiOver4 >> 7, c2 >> 2);  // Q24
    i32 pulse = base + multiply(base, poly) * 2;
    const u32 steps = ((static_cast<u32>(strike_control < 0 ? 0 : strike_control) >> 16) * 11 + (1u << 14)) >> 15;  // 0..11: up to 2048 samples, 46 ms
    e.noise = steps > 0;
    e.burst = 1u << steps;
    e.pulse = multiply(pulse, kBurstGain[steps]);
    e.active = 1;
}

// One block of the strike filter into the resonator's input, Q7.24.
inline void excite(Voice& voice, i32* input) {
    Exciter& e = voice.exciter;
    i32 s1 = e.s1, s2 = e.s2;
    const i32 g = e.g, h = e.h;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 injected = 0;
        if (e.burst) {
            --e.burst;
            const i32 sample = e.noise ? noise(voice.seed) : kOne;
            injected = multiply(h, multiply(e.pulse, sample));
        }
        const i32 v = -(s2 + multiply(kExciterR, s1) + multiply(g, s1));
        const i32 t = injected + multiply(g, multiply(h, v));
        const i32 bp = t + s1;
        s1 = bp + t;
        const i32 u = multiply(g, bp);
        const i32 lp = u + s2;
        s2 = lp + u;
        input[i] = opxy::q31::half(lp);
    }
    e.s1 = s1;
    e.s2 = s2;
    const i32 floor = 1 << 8;
    if (!e.burst && s1 < floor && s1 > -floor && s2 < floor && s2 > -floor) {
        e.s1 = e.s2 = 0;
        e.active = 0;
    }
}
#include "strings.h"

// Structure for the modal and string bodies, with Rings' harmonic zone (its
// flat spot at a quarter) moved to the knob's centre: 0..45 compresses up to
// it, 45..55 rests on it, 55..100 stretches towards a bell.
inline i32 harmonic_centre(i32 structure) {
    const i32 s = structure < 0 ? 0 : structure;
    if (s < 966367641) return multiply(s, 1193046471);                        // 0.25 / 0.45
    if (s < 1181116006) return 536870912 + multiply(s - 966367641, kTenth);  // 0.25 + 0.1 (s - 0.45)
    const i32 above = multiply(s - 1181116006, 1765657446) * 2;              // 1.644 (s - 0.55), at most 0.74
    return above > kOne - 558345748 ? kOne : 558345748 + above;              // 0.26 + above
}

// The Model enum choice from its Q31 word.
inline u32 model_choice(i32 control) {
    u32 choice = (static_cast<u32>(control < 0 ? 0 : control) >> 16) * kModels >> 15;
    return choice >= kModels ? kModels - 1 : choice;
}

// Rings' limiter: pre-gain in, hold the block's peak, release slowly, 0.8 out, then our half.
inline void limit(Voice& voice, const i32* sum, i32* output, i32 pre_gain_extra, i32 level_out) {
    i32 largest = 0;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const i32 magnitude = sum[i] < 0 ? -sum[i] : sum[i];
        if (magnitude > largest) largest = magnitude;
    }
    const i32 peak = largest + multiply(largest, pre_gain_extra);
    if (peak > voice.peak) voice.peak = peak;
    else voice.peak -= multiply(voice.peak - peak, kRelease);
    i32 gain = kOne;
    if (voice.peak > kUnity) gain = divide(kUnity, reciprocal(static_cast<u32>(voice.peak)), 0);
    const i32 level = multiply(level_out, gain);
    for (unsigned i = 0; i < opxy::block_frames; ++i)
        output[i] = clamp(multiply(sum[i], level), -(1 << 23), 1 << 23) * 128;
}

// One block of the string models: the plucker into the main string, and for
// the sympathetic models three more strings fed by the main string's bridge.
inline void render_strings(Voice& voice, u32 model, i32 structure, i32 brightness, i32 damping, i32 position,
                           u32 increment, u32 strings, bool dirty, i32* sum) {
    const i32 stiffness = harmonic_centre(structure);  // the main string's dispersion; the notes keep the raw knob
    const u32 count = strings;
    for (u32 n = count; n < kStrings; ++n) voice.strings[n].active = 0;
    if (!voice.ringing) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) sum[i] = 0;
        return;
    }
    i32 input[opxy::block_frames];
    i32 bridge[opxy::block_frames];
    pluck_render(voice, input);
    // The string model's extras are unison copies a few cents apart, a
    // thickening that is not in Rings; the sympathetic models tune theirs.
    static const i32 kDetune[kStrings] = {0, 1565873, -2013266, 2684354};  // 0, +7, -9, +12 cents, Q28 octaves
    i32 offsets[kStrings] = {0, 0, 0, 0};
    if (model == kString) { for (u32 n = 0; n < kStrings; ++n) offsets[n] = kDetune[n]; }
    else sympathetic_notes(structure, model, offsets);  // chords tune the main string too
    for (u32 n = 0; n < count; ++n) {
        String& s = voice.strings[n];
        if (!s.active) {
            // Back in the bank: silent, in tune at once, and tuned below.
            for (unsigned i = 0; i < kLineLength; ++i) s.line.samples[i] = 0;
            s.x1 = s.x2 = s.s1 = s.s2 = s.dc_x = s.dc_y = s.noise = s.bridge = 0;
            s.increment = 0;
            s.active = 1;
            dirty = true;
        }
        i32 bright = brightness, damp = damping;
        bool retune = dirty;
        if (n == 0) {
            // Chords tune the main string an octave below the note, as Rings'
            // chord tables do; the sympathetic model leaves it on the note.
            s.increment = s.target = offsets[0] ? static_cast<u32>(exp2_scale(static_cast<i32>(increment >> 1), offsets[0])) << 1 : increment;
        } else {
            // Sympathetic strings glide to their notes, duller and well damped.
            s.target = static_cast<u32>(exp2_scale(static_cast<i32>(increment >> 1), offsets[n])) << 1;
            if (model != kString) {
                bright = bend(bend(brightness));
                damp = 1503238554 + multiply(damping, 579820585);                              // 0.7 + 0.27 d
            }
            const i32 glide = exp2_scale(kOne, ((bright - kOne) >> 3) * 3);                     // 2^((b - 1) 3)
            const i32 gap = static_cast<i32>(s.target - s.increment);
            const u32 before = s.increment;
            if (!s.increment) s.increment = s.target;
            else if (gap > -64 && gap < 64) s.increment = s.target;
            else s.increment += static_cast<u32>(multiply(gap, glide));
            retune = dirty || s.increment != before;  // a full retune only while gliding or after a control moved
        }
        const i32 index = static_cast<i32>(n * (2147483648u / count));                        // n / count, Q31
        if (model != kString) damp += multiply(index, 2040109466 - damp);                     // towards 0.95 up the bank
        if (n == 0) {
            if (model == kString) {
                if (retune) tune_string(s, &voice.stretch, stiffness, bright, damp, position);
                // Structure's harmonic zone has no dispersion: the plain loop then.
                if (s.stretch_point || s.noise_amount || s.bridge_curving || s.ac_blocking)
                    render_string<kMainStretch>(s, voice.stretch, voice.seed, input, sum, bridge);
                else
                    render_string<kMain>(s, voice.stretch, voice.seed, input, sum, bridge);
            } else {
                if (retune) tune_string(s, 0, stiffness, bright, damp, position);
                render_string<kMain>(s, voice.stretch, voice.seed, input, sum, bridge);
            }
        } else {
            if (retune) tune_string(s, 0, stiffness, bright, damp, position);
            render_string<kCopy>(s, voice.stretch, voice.seed, bridge, sum, 0);
        }
        if (n == 0 && count > 1) {
            // The other strings hear the bridge, as in Rings, and half the pluck
            // itself, as Rings' strings do from an external exciter: with the
            // internal one alone they barely spoke, and the bank's size was lost.
            const i32 feed = 107374182;                                                        // 0.2 / 4
            if (model == kString) for (unsigned i = 0; i < opxy::block_frames; ++i) bridge[i] = input[i];  // unison copies: the pluck itself
            else for (unsigned i = 0; i < opxy::block_frames; ++i) bridge[i] = multiply(bridge[i], feed) + (input[i] >> 1);
        }
    }
    // Rung down below -120 dB with the pluck spent: nothing until the next note.
    i32 largest = 0;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const i32 magnitude = sum[i] < 0 ? -sum[i] : sum[i];
        if (magnitude > largest) largest = magnitude;
    }
    if (!voice.plucker.remaining && largest < kIdle && damping < 2040109466) voice.ringing = 0;
}
}  // namespace

// Callbacks run serially per voice; owner is a stable nonzero voice ID.
inline void initialize(Voice& voice, Shared&, opxy::u32 owner) {
    voice.seed = owner * 2654435761u | 1u;
    voice.structure = voice.brightness = voice.damping = voice.position = -1;  // never a control
    voice.modes_control = voice.pitch = voice.model = 0xffffffffu;
    voice.pending = 0;
    voice.peak = 0;
    voice.count = 0;
    voice.exciter.active = 0;
    voice.ringing = voice.gliding = 0;
    voice.plucker.remaining = 0;
    for (u32 n = 0; n < kStrings; ++n) voice.strings[n].increment = voice.strings[n].active = 0;
}
inline void start(Voice& voice, Shared&, opxy::u32) { voice.pending = 1; }
inline void destroy(Voice&, Shared&, opxy::u32) {}

inline void render(Voice& voice, Shared&, opxy::u32, opxy::Output output,
                   opxy::Parameters parameters, opxy::Pitch pitch) {
    const i32 structure = parameters[0], brightness = parameters[1];
    const i32 damping = parameters[2], position = parameters[3];
    const u32 model = model_choice(parameters[4]);
    const u32 modes_control = static_cast<u32>(parameters[6] < 0 ? 0 : parameters[6]);
    const u32 increment = pitch[0];
    i32 sum[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) sum[i] = 0;
    const bool dirty = structure != voice.structure || brightness != voice.brightness || damping != voice.damping ||
                       position != voice.position || modes_control != voice.modes_control || increment != voice.pitch ||
                       model != voice.model;
    if (model != kModal) {
        if (voice.pending) {
            pluck(voice, brightness, increment, position, parameters[5]);
            voice.ringing = 1;
            voice.pending = 0;
        }
        render_strings(voice, model, structure, brightness, damping, position, increment, string_count(static_cast<i32>(modes_control)), dirty, sum);
        voice.structure = structure; voice.brightness = brightness; voice.damping = damping;
        voice.position = position; voice.modes_control = modes_control; voice.pitch = increment; voice.model = model;
        // Level trims so the strings strike about as loud as the modal body
        // (measured at the defaults): string 1.6; sympathetic 1.8 alone and
        // 1.2 with more strings; chords 2 alone and 1.6 with more.
        static const i32 kTrimHalf[kModels][kStrings] = {
            {0, 0, 0, 0},
            {1717986918, 1717986918, 1717986918, 1717986918},
            {1932735283, 1288490189, 1288490189, 1288490189},
            {2147483647, 1717986918, 1717986918, 1717986918}};
        const u32 strings = string_count(static_cast<i32>(modes_control));
        for (unsigned i = 0; i < opxy::block_frames; ++i) sum[i] = multiply(sum[i], kTrimHalf[model][strings - 1]) * 2;
        if (model == kString) limit(voice, sum, output, kPoint4, kOutput);
        else limit(voice, sum, output, 0, kOutputUnity);
        return;
    }
    if (voice.pending) {
        strike(voice, brightness, increment, parameters[5]);
        voice.pending = 0;
    }
    if (dirty) {
        tune(voice, harmonic_centre(structure), brightness, damping, position, mode_count(static_cast<i32>(modes_control)), mode_ease(static_cast<i32>(modes_control)), increment);
        voice.structure = structure;
        voice.brightness = brightness;
        voice.damping = damping;
        voice.position = position;
        voice.modes_control = modes_control;
        voice.pitch = increment;
        voice.model = model;
    }
    i32 input[opxy::block_frames];
    const bool excited = voice.exciter.active != 0;
    if (excited) excite(voice, input);
    else for (unsigned i = 0; i < opxy::block_frames; ++i) input[i] = 0;
    for (u32 m = 0; m < voice.count; ++m) {
        Mode& mode = voice.modes[m];
        if (!mode.active) continue;
        i32 x = mode.x, y = mode.y;
        // A mode that has rung down below -120 dB costs nothing until the next strike.
        if (!excited && x < kIdle && x > -kIdle && y < kIdle && y > -kIdle) continue;
        const i32 k = mode.rotation, a = mode.amplitude, b = mode.drive;
        if (mode.heavy) {
            const i32 keep = mode.decay;
            if (excited) {
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    x += multiply(b, input[i]);
                    x += multiply(k, y) * 2;
                    y -= multiply(k, x) * 2;
                    x = multiply(keep, x);
                    y = multiply(keep, y);
                    sum[i] += multiply(a, x);
                }
            } else {
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    x += multiply(k, y) * 2;
                    y -= multiply(k, x) * 2;
                    x = multiply(keep, x);
                    y = multiply(keep, y);
                    sum[i] += multiply(a, x);
                }
            }
        } else {
            x = lose(x, mode.decay, mode.carry_x);
            y = lose(y, mode.decay, mode.carry_y);
            if (excited) {
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    x += multiply(b, input[i]);
                    x += multiply(k, y) * 2;
                    y -= multiply(k, x) * 2;
                    sum[i] += multiply(a, x);
                }
            } else {
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    x += multiply(k, y) * 2;
                    y -= multiply(k, x) * 2;
                    sum[i] += multiply(a, x);
                }
            }
        }
        mode.x = x;
        mode.y = y;
    }
    limit(voice, sum, output, kPoint4, kOutput);
}
}
