// Rings' string models (rings/dsp/string.cc and part.cc, MIT, Emilie Gillet)
// in fixed point: a Karplus-Strong loop read back a fractional delay with a
// Hermite interpolator, damped by a brightness FIR and a low-pass, with
// Structure's dispersion as an all-pass stretch, noise and a curved bridge.
// Included by engine.cpp after its fixed-point helpers; not a standalone header.

// --- Loops -----------------------------------------------------------------
inline void line_write(Line& line, i32 sample) {
    line.samples[line.write] = sample;
    line.write = (line.write - 1) & (kLineLength - 1);
}
// Linear read at a Q16 delay.
inline i32 line_read(const Line& line, i32 delay) {
    const u32 whole = static_cast<u32>(delay >> 16);
    const i32 fraction = static_cast<i32>(delay & 0xffff) << 15;
    const i32 a = line.samples[(line.write + whole) & (kLineLength - 1)];
    const i32 b = line.samples[(line.write + whole + 1) & (kLineLength - 1)];
    return a + multiply(b - a, fraction);
}
// Hermite read at a Q16 delay, as stmlib's DelayLine::ReadHermite.
inline i32 line_read_hermite(const Line& line, i32 delay) {
    const u32 t = line.write + static_cast<u32>(delay >> 16) + kLineLength;
    const i32 f = static_cast<i32>(delay & 0xffff) << 15;
    const i32 xm1 = line.samples[(t - 1) & (kLineLength - 1)];
    const i32 x0 = line.samples[t & (kLineLength - 1)];
    const i32 x1 = line.samples[(t + 1) & (kLineLength - 1)];
    const i32 x2 = line.samples[(t + 2) & (kLineLength - 1)];
    const i32 c = (x1 - xm1) >> 1;
    const i32 v = x0 - x1;
    const i32 w = c + v;
    const i32 a = w + v + ((x2 - x0) >> 1);
    const i32 b_neg = w + a;
    return multiply(multiply(multiply(a, f) - b_neg, f) + c, f) + x0;
}
// First-order all-pass through the stretch loop at a whole-sample delay.
inline i32 stretch_allpass(Stretch& stretch, i32 sample, u32 delay, i32 coefficient) {
    const i32 read = stretch.samples[(stretch.write + delay) & (kStretchLength - 1)];
    const i32 write = sample + multiply(coefficient, read);
    stretch.samples[stretch.write] = write;
    stretch.write = (stretch.write - 1) & (kStretchLength - 1);
    return read - multiply(coefficient, write);
}

// --- Per-block tuning --------------------------------------------------------
const i32 kDcPole = 2146509803;          // 1 - 20 / 44100
const i32 kInvPi = 683565276;            // 1 / pi
const i32 kBridgeKnee = 419430;          // 0.025 in Q7.24
const i32 kDelayMin = 4 << 16, kDelayMax = (kLineLength - 4) << 16;
const i32 kStringCutoffLimit = kCutoffLimit;  // the damping low-pass's reach, as the strike filter's

// The loop length for a pitch: 2^48 / increment in Q16 samples, within the line.
inline i32 loop_delay(u32 increment) {
    if (increment < 1u << 10) return kDelayMax;
    const Reciprocal r = reciprocal(increment);
    return clamp(shift(r.r, r.e - 14), kDelayMin, kDelayMax);
}

// tan(pi f) for f in Q31 cycles per sample, Q30: the bilinear warp of a frequency.
inline i32 tan_pi(i32 f) {
    const i32 t = f + multiply(f, kHalfPiLessOne);             // pi f, Q30
    const i32 t2 = multiply(t, t) * 2;
    const i32 t4 = multiply(t2, t2) * 2;
    const i32 poly = multiply(t2, kThird) + multiply(t4, kTwoFifteenths);
    return t + multiply(t, poly) * 2;
}

// The low-pass at cutoff (Q31 cycles per sample) with 1/Q = r2 halves (r = r2 / 2).
inline void lowpass(i32 cutoff, u32 r2, i32& g, i32& h) {
    cutoff = clamp(cutoff, 1024, kStringCutoffLimit);
    const i32 g30 = tan_pi(cutoff);                            // tan(pi fc), Q30
    g = g30 * 2;
    // 1 + r g + g^2 in Q29
    const u32 den = (1u << 29) + (static_cast<u32>(g30) >> 1) * r2 / 2 + static_cast<u32>(multiply(g30, g30));
    h = divide(1 << 30, reciprocal(den), -1);
}

// Configure one string for the block from the controls, as String::ProcessInternal does.
inline void tune_string(String& s, Stretch* stretch, i32 structure, i32 brightness, i32 damping, i32 position) {
    const i32 delay = loop_delay(s.increment);
    // Dispersion from Structure: none around a quarter, bridge curving below, stretch above.
    i32 dispersion = 0;
    const i32 kLow = 515396076, kHigh = 558345748;             // 0.24, 0.26
    if (structure < kLow) dispersion = multiply(structure - kLow, 2147483647) * 4 + multiply(structure - kLow, 356482286);  // (s - 0.24) 4.166
    else if (structure > kHigh) dispersion = structure - kHigh + multiply(structure - kHigh, 754599288);                    // (s - 0.26) 1.35135
    dispersion = clamp(dispersion, -kOne, kOne);
    if (!stretch) dispersion = 0;
    const i32 magnitude = dispersion < 0 ? -dispersion : dispersion;
    s.stretch_point = dispersion > 0 ? multiply(bend(dispersion), 1020054733) : 0;          // d (2 - d) 0.475
    i32 noise_amount = dispersion > 1610612736 ? (dispersion - 1610612736) * 4 : 0;          // 4 (d - 0.75)
    s.noise_amount = multiply(multiply(noise_amount, noise_amount), 53687091);              // ^2 0.025
    const i32 curving = dispersion < 0 ? -dispersion : 0;
    s.ac_blocking = curving;
    s.bridge_curving = multiply(multiply(curving, curving), 21474836);                       // ^2 0.01
    // ap_gain = -0.618 d / (0.15 + |d|)
    {
        const u32 den = 322122547u + static_cast<u32>(magnitude);                           // Q31
        const i32 ratio = divide(dispersion, reciprocal(den), 0);
        s.ap_gain = -multiply(ratio, 1327144185);
    }
    // Damping: the loop's gain per pass from an RT60 that Damping sets.
    const i32 lf = bend(damping);                                                            // d (2 - d)
    // The loop's gain per pass: 2^(-(delay / 308.7) 2^(-8 lf)), from an RT60 of
    // 0.07 s x 2^(8 lf). delay x 13.268 is that exponent in Q28 octaves.
    const i32 base = delay * 13 + (delay >> 13) * 2195;
    i32 e = -exp2_scale(base, -lf);                                                          // -lf as Q28 is -8 lf
    if (e < -(7 << 28)) e = -(7 << 28);
    i32 coefficient = exp2_scale(kOne, e);
    i32 bright = multiply(brightness, brightness);
    s.noise_filter = exp2_scale(kOne, (brightness - kOne) / 2);                              // 2^((b - 1) 4)
    {
        const u32 den = 429496730u + static_cast<u32>(s.noise_filter);                      // 0.2 + nf, Q31
        s.noise_gain = divide(1 << 29, reciprocal(den), -1);                                // 1 / (0.2 + nf), Q28
    }
    const i32 d2 = multiply(damping, damping);
    i32 cutoff_oct = (d2 >> 1) + (bright >> 2) + (1 << 29);                                 // (24 + 48 d^2 + 24 b^2) / 12 octaves, Q28
    if (cutoff_oct > 7 << 28) cutoff_oct = 7 << 28;
    i32 damping_f = exp2_scale(static_cast<i32>(s.increment >> 1), cutoff_oct);            // f 2^(cutoff / 12), Q31
    if (damping_f > kStringCutoffLimit) damping_f = kStringCutoffLimit;
    if (damping >= 2040109466) {                                                             // 0.95: towards infinite sustain
        const i32 to_infinite = (damping - 2040109466) * 20;
        coefficient += multiply(to_infinite, kOne - coefficient);
        bright += multiply(to_infinite, kOne - bright);
        damping_f += multiply(to_infinite, kOne - damping_f);
        cutoff_oct += multiply(to_infinite, (7 << 28) - cutoff_oct);
    }
    const i32 h0 = (kOne >> 1) + (bright >> 1);                                              // (1 + b) / 2
    const i32 h1 = (kOne >> 2) - (bright >> 2);                                              // (1 - b) / 4
    s.c0 = multiply(coefficient, h0);
    s.c1 = multiply(coefficient, h1);
    lowpass(damping_f, 4, s.g, s.h);                                                         // Q 0.5
    // The low-pass's phase delay at the note, atan(tan(pi f) / tan(pi fc)) / pi of
    // a period, shortens the loop. Rings tabulates it from the cutoff's
    // semitones; the warped ratio keeps it right where the cutoff is capped.
    const i32 note_warp = tan_pi(static_cast<i32>(s.increment >> 1));
    i32 shift_arg = divide(note_warp, reciprocal(static_cast<u32>(tan_pi(damping_f))), 0);
    if (shift_arg > 1073741824) shift_arg = 1073741824;
    const i32 x3 = multiply(multiply(shift_arg, shift_arg), shift_arg);
    const i32 compensation = kOne - multiply(shift_arg - multiply(x3, kThird), kInvPi);
    i32 loop = multiply(delay, compensation) - (1 << 16);                                    // less the FIR's sample
    if (loop < kDelayMin) loop = kDelayMin;
    s.delay = loop;
    // Position's pickup along the loop.
    const i32 off_centre = position - (kOne >> 1);
    const i32 pickup = (kOne >> 1) - multiply(off_centre < 0 ? -off_centre : off_centre, 2104533974);  // 0.5 - 0.98 |p - 0.5|
    s.comb = pickup;
}

// --- Rendering -------------------------------------------------------------
// One block of a string: input in, out += s - pickup, as Rings dispatches voices.
inline void render_string(String& s, Stretch* stretch, u32& seed, const i32* input, i32* out, i32* bridge_out) {
    const i32 c0 = s.c0, c1 = s.c1, g = s.g, h = s.h, comb = s.comb;
    i32 x1 = s.x1, x2 = s.x2, s1 = s.s1, s2 = s.s2, dc_x = s.dc_x, dc_y = s.dc_y;
    i32 dnoise = s.noise, bridge = s.bridge;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 delay = s.delay;
        i32 sample;
        if (stretch) {
            // Noise and the curved bridge modulate the loop length.
            const i32 n = multiply(noise(seed) >> 7, s.noise_gain) * 8;                       // +-1/(0.2 + nf), Q24
            dnoise += multiply(s.noise_filter, n - dnoise);
            const i32 wobble = multiply(dnoise, s.noise_amount) - multiply(bridge, s.bridge_curving);  // Q24
            delay += multiply(delay, clamp(wobble, -(1 << 23), 1 << 23) << 7);
            const i32 ap_delay = multiply(delay, s.stretch_point);
            const i32 main_delay = delay - ap_delay;
            if (ap_delay >= kDelayMin && main_delay >= kDelayMin) {
                sample = line_read_hermite(s.line, main_delay);
                sample = stretch_allpass(*stretch, sample, static_cast<u32>(ap_delay >> 16), s.ap_gain);
            } else {
                sample = line_read_hermite(s.line, delay);
            }
            // The bridge: DC blocked by Structure below a quarter, and bent by the signal.
            const i32 ac = sample - dc_x + multiply(kDcPole, dc_y);
            dc_x = sample;
            dc_y = ac;
            sample += multiply(s.ac_blocking, ac - sample);
            const i32 magnitude = sample < 0 ? -sample : sample;
            const i32 value = magnitude - kBridgeKnee;
            const i32 positive = value > 0 ? value * 2 : 0;
            bridge = sample > 0 ? positive : -(positive + (positive >> 1));
        } else {
            sample = line_read_hermite(s.line, delay);
        }
        sample += input[i];
        // The damping FIR, then the damping low-pass.
        const i32 fir = multiply(c0, x1) + multiply(c1, sample + x2);
        x2 = x1;
        x1 = sample;
        const i32 v = fir - (s2 + s1 * 2 + multiply(g, s1));
        const i32 t = multiply(g, multiply(h, v));
        const i32 bp = t + s1;
        s1 = bp + t;
        const i32 u = multiply(g, bp);
        i32 lp = u + s2;
        s2 = lp + u;
        // Eight units of headroom: a sustained drive at infinite damping gathers
        // without bound in Rings' float loop; here it stops short of the integer.
        lp = clamp(lp, -(1 << 27), 1 << 27);
        line_write(s.line, lp);
        const i32 pickup = line_read(s.line, multiply(delay, comb));
        const i32 o = lp - pickup;
        out[i] += o;
        if (bridge_out) bridge_out[i] = o;
    }
    s.x1 = x1; s.x2 = x2; s.s1 = s1; s.s2 = s2; s.dc_x = dc_x; s.dc_y = dc_y;
    s.noise = dnoise; s.bridge = bridge;
}

// --- The plucker and the sympathetic strings' notes ------------------------
// Arm the plucker for a note: Rings' Plucker::Trigger.
inline void pluck(Voice& voice, i32 brightness, u32 increment, i32 position) {
    Plucker& p = voice.plucker;
    const i32 ratio = 107374182 + multiply(position, 1932735283);        // 0.05 + 0.9 p
    i32 period = multiply(loop_delay(increment), ratio);                 // Q16 samples
    p.remaining = static_cast<u32>(period >> 16);
    while (period >= 255 << 16) period >>= 1;
    p.period = period;
    p.gain = multiply(kOne - position, 1717986918);                      // 0.8 (1 - p)
    // Its low-pass: eight times the strike filter's cutoff, at Q 1.
    const i32 colour = bend(brightness);
    i32 cutoff = shift(exp2_scale(static_cast<i32>(increment >> 1), colour - (1 << 30)), 3);
    if (cutoff > kStringCutoffLimit) cutoff = kStringCutoffLimit;
    lowpass(cutoff, 2, p.g, p.h);
}
// One block of the plucker, DC blocked, into the main string's input, Q7.24.
inline void pluck_render(Voice& voice, i32* out) {
    Plucker& p = voice.plucker;
    const i32 g = p.g, h = p.h, gain = p.gain, period = p.period;
    i32 s1 = p.s1, s2 = p.s2, dc_x = voice.in_dc_x, dc_y = voice.in_dc_y;
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        i32 sample = 0;
        if (p.remaining) {
            --p.remaining;
            sample = noise(voice.seed) >> 7;
        }
        const u32 whole = static_cast<u32>(period >> 16);
        const i32 fraction = static_cast<i32>(period & 0xffff) << 15;
        const i32 a = p.comb[(p.write + whole) & (kCombLength - 1)];
        const i32 b = p.comb[(p.write + whole + 1) & (kCombLength - 1)];
        sample += multiply(gain, a + multiply(b - a, fraction));
        p.comb[p.write] = sample;
        p.write = (p.write - 1) & (kCombLength - 1);
        const i32 v = sample - (s2 + s1 + multiply(g, s1));
        const i32 t = multiply(g, multiply(h, v));
        const i32 bp = t + s1;
        s1 = bp + t;
        const i32 u = multiply(g, bp);
        const i32 lp = u + s2;
        s2 = lp + u;
        const i32 y = lp - dc_x + multiply(kDcPole, dc_y);
        dc_x = lp;
        dc_y = y;
        out[i] = y;
    }
    p.s1 = s1; p.s2 = s2; voice.in_dc_x = dc_x; voice.in_dc_y = dc_y;
    const i32 floor = 1 << 6;
    if (!p.remaining && s1 < floor && s1 > -floor && s2 < floor && s2 > -floor) { p.s1 = p.s2 = 0; }
}
// Part::Squash: a steep S-curve that snaps a fraction towards 0 or 1.
inline i32 squash(i32 x) {
    const bool upper = x >= (kOne >> 1);
    i32 y = upper ? (kOne - x) * 2 : x * 2;
    for (int i = 0; i < 4; ++i) y = multiply(y, y);
    y >>= 1;
    return upper ? kOne - y : y;
}
// The sympathetic strings' notes as octave offsets from the played note, Q28:
// Rings' ComputeSympatheticStringsNotes for four strings.
inline void sympathetic_notes(i32 structure, u32 model, i32* offsets) {
    const i32 kSemitone = 22369621;  // 2^28 / 12
    if (model == kChords) {
        static const short chords[11][4] = {
            {-1200, 0, 1, 1200}, {-1200, 300, 700, 1000}, {-1200, 300, 700, 1200}, {-1200, 300, 700, 1400},
            {-1200, 300, 700, 1700}, {-1200, 700, 1200, 1900}, {-1200, 400, 700, 1700}, {-1200, 400, 700, 1400},
            {-1200, 400, 700, 1200}, {-1200, 400, 700, 1100}, {-1200, 500, 700, 1200}};
        u32 chord = (static_cast<u32>(structure < 0 ? 0 : structure) >> 16) * 11 >> 15;
        if (chord > 10) chord = 10;
        for (u32 i = 0; i < 4; ++i) offsets[i] = chords[chord][i] * 223696;  // cents to Q28 octaves
        return;
    }
    // The ladder: tonic, an octave down, a fifth down, the note, a fifth, an octave, a twelfth, two octaves.
    static const i32 ladder[9] = {0, -(1 << 28), -157035123, 0, 157035123, 1 << 28, 425470579, 2 << 28, 2 << 28};
    i32 p = structure < 0 ? 0 : structure;
    offsets[0] = 0;
    for (u32 i = 1; i < 3; ++i) {
        const i32 note = multiply(p, 7 << 28);                          // 0..7, Q28
        const u32 index = static_cast<u32>(note >> 28);
        const i32 fraction = squash((note & ((1 << 28) - 1)) << 3);
        const i32 a = ladder[index], b = ladder[index + 1];
        offsets[i] = a + multiply(b - a, fraction);
        p += multiply(kOne - p, kFifth);
    }
    offsets[3] = offsets[0] + kSemitone * 13 / 1000;                   // a 0.013 semitone twin
}
