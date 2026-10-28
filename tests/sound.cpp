// The machine's own sound checks, compiled for the host and run by
// python sdk.py test --machine <folder>: a struck note rings and decays, sits
// in tune, obeys the controls, stays bounded in every corner, and falls silent.
#include "../engine.cpp"
#include <cstdio>
#include <cmath>

namespace {
const unsigned N = opxy::block_frames;
const opxy::i32 kPeak = 1073741824;  // definition.json's outputPeakQ31
int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL %s\n", what); ++failures; }
}

opxy::i32 control(double v) { return static_cast<opxy::i32>(v * 2147483647.0 + 0.5); }
opxy::u32 increment(double hz) { return static_cast<opxy::u32>(hz / 44100.0 * 4294967296.0 + 0.5); }

struct Note {
    mi_rings::Voice voice;
    mi_rings::Shared shared;
    opxy::i32 parameters[8];
    opxy::u32 pitch[N];
    opxy::i32 output[N];
    double peak;  // the largest |sample| ever rendered, Q31 scale

    Note(double hz, double structure, double bright, double damping, double position, double modes, double strike,
         double model = 0.125) : peak(0) {
        parameters[0] = control(structure); parameters[1] = control(bright); parameters[2] = control(damping);
        parameters[3] = control(position); parameters[4] = control(model); parameters[5] = control(strike);
        parameters[6] = control(modes); parameters[7] = 0;
        for (unsigned i = 0; i < N; ++i) pitch[i] = increment(hz);
        unsigned char* bytes = reinterpret_cast<unsigned char*>(&voice);
        for (unsigned i = 0; i < sizeof voice; ++i) bytes[i] = 0;
        mi_rings::initialize(voice, shared, 1);
        mi_rings::start(voice, shared, 1);
    }
    // Renders blocks; returns their RMS in units of full scale.
    double render(unsigned blocks, double* samples = 0) {
        double energy = 0;
        for (unsigned b = 0; b < blocks; ++b) {
            mi_rings::render(voice, shared, 1, output, parameters, pitch);
            for (unsigned i = 0; i < N; ++i) {
                const double s = output[i] / 2147483648.0;
                if (samples) samples[b * N + i] = s;
                energy += s * s;
                if (s > peak) peak = s;
                if (-s > peak) peak = -s;
                check(output[i] >= -kPeak && output[i] <= kPeak, "above the declared peak");
            }
        }
        return std::sqrt(energy / (blocks * N));
    }
};
}

int main() {
    const double modes12 = 0.375;
    {   // The exponent helper floors at every scale: 2^(-7.2) from a Q31 loss of
        // 0.9 read as Q28, where -x + 2^28 overflows a 32-bit int on the device.
        const opxy::i32 kOne = 2147483647;
        const opxy::i32 lf = control(0.9);
        const double expected = std::pow(2.0, -7.2) * 2147483648.0;
        const double got = mi_rings::exp2_scale(kOne, -lf);
        check(std::fabs(got - expected) < expected * 0.01, "exp2_scale wrong for a large negative exponent");
    }
    {   // A default note: a clear strike that decays over a second.
        Note note(440, 0.5, 0.5, 0.6, 0, modes12, 0);
        const double early = note.render(4);          // 6 ms
        const double middle = note.render(30);        // to 50 ms
        for (unsigned b = 0; b < 300; ++b) note.render(1);  // to 0.5 s
        const double late = note.render(30);
        check(early > 0.01, "strike too quiet");
        check(middle < early, "no early decay");
        check(late < middle * 0.5, "no decay by half a second");
        check(note.peak <= 0.5, "louder than half scale");
    }
    {   // In tune: a harmonic series repeats every period (autocorrelation at 100.2 samples).
        Note note(440, 0.5, 0.5, 0.7, 0, modes12, 0);
        note.render(8);
        double s[64 * N];
        note.render(64, s);
        double same = 0, shifted = 0, lagged = 0;
        for (unsigned i = 0; i < 64 * N - 101; ++i) {
            const double at = s[i], later = s[i + 100] * 0.8 + s[i + 101] * 0.2;
            same += at * at; shifted += later * later; lagged += at * later;
        }
        check(lagged / std::sqrt(same * shifted) > 0.9, "not periodic at the note");
    }
    {   // Damping: more of it rings longer; Modes: more of them cost more energy at the top.
        Note dry(220, 0.5, 0.5, 0.2, 0, modes12, 0), wet(220, 0.5, 0.5, 0.9, 0, modes12, 0);
        dry.render(4); wet.render(4);
        for (unsigned b = 0; b < 200; ++b) { dry.render(1); wet.render(1); }
        check(wet.render(20) > dry.render(20) * 10, "Damping does not lengthen the ring");
        Note few(220, 0.5, 0.5, 0.6, 0, 0, 0), many(220, 0.5, 0.5, 0.6, 0, 1, 0);
        few.render(1); many.render(1);
        check(few.voice.count == 2 && many.voice.count == 24, "Modes count");
    }
    {   // Position at the middle strike mutes every second partial: no energy at 880 Hz' neighbours? Check it differs.
        Note end(440, 0.5, 0.5, 0.6, 0, modes12, 0), middle(440, 0.5, 0.5, 0.6, 1, modes12, 0);
        end.render(1); middle.render(1);
        check(end.voice.modes[1].amplitude > 0x7f000000 && middle.voice.modes[1].amplitude < 0x01000000, "Position weights");
    }
    {   // Structure: harmonic at 0.5, stretched above, compressed below.
        Note harmonic(110, 0.5, 0.5, 0.6, 0, modes12, 0), bell(110, 0.8, 0.5, 0.6, 0, modes12, 0), tight(110, 0, 0.5, 0.6, 0, modes12, 0);
        harmonic.render(1); bell.render(1); tight.render(1);
        const opxy::i32 h = harmonic.voice.modes[3].rotation, b = bell.voice.modes[3].rotation, t = tight.voice.modes[3].rotation;
        check(b > h && t < h, "Structure direction");
        const double ratio = std::asin(h / 2147483647.0) / std::asin(harmonic.voice.modes[0].rotation / 2147483647.0);
        check(ratio > 3.99 && ratio < 4.01, "fourth partial not at four times the note");
    }
    {   // Every corner, low and high, with the noise strike, stays bounded and ends quiet.
        const double hz[2] = {27.5, 4186};
        for (unsigned corner = 0; corner < 64; ++corner) {
            const double st = (corner & 1) ? 1 : 0, br = (corner & 2) ? 1 : 0, da = (corner & 4) ? 1 : 0;
            const double po = (corner & 8) ? 1 : 0, mo = (corner & 16) ? 1 : 0, strike = (corner & 32) ? 1 : 0;
            for (unsigned p = 0; p < 2; ++p) for (unsigned model = 0; model < 4; ++model) {
                Note note(hz[p], st, br, da, po, mo, strike, 0.125 + 0.25 * model);
                note.render(40);
                mi_rings::start(note.voice, note.shared, 1);  // a restrike on a ringing voice
                note.render(40);
                note.parameters[0] = control(0.5); note.parameters[3] = control(0.3);  // a turn mid-note
                note.render(40);
            }
        }
    }
    {   // The string models: a pluck that rings, a tuned string, and sympathetic strings that answer.
        static const char* const names[3] = {"string", "sympathetic", "chords"};
        for (unsigned model = 1; model < 4; ++model) {
            Note note(220, 0.5, 0.6, 0.6, 0.3, modes12, 0, 0.125 + 0.25 * model);
            double early = note.render(8);
            double swell = note.render(60);   // sympathetic strings gather energy for a second or so
            double mid = note.render(600);
            double late = note.render(1500);
            std::printf("%s: early %.4f swell %.4f mid %.4f late %.4f peak %.3f\n", names[model - 1], early, swell, mid, late, note.peak);
            check(early > 0.005, "pluck too quiet");
            check(late < mid && (model != 1 || late < early), "string does not decay");
        }
        {   // Infinite sustain with sympathetic strings gathering energy for ten seconds stays bounded.
            Note note(110, 0.45, 0.7, 1, 0.3, modes12, 0, 0.625);
            note.render(6890);
            check(note.peak <= 0.5, "sympathetic sustain unbounded");
        }
        // The plain string's pitch: the autocorrelation peak of a long ring sits at the period of 220 Hz.
        Note note(220, 0.5, 0.6, 0.8, 0.3, modes12, 0, 0.375);  // Struct 0.5: no dispersion to bend the pitch
        note.render(4);
        static double samples[N * 64];
        note.render(64, samples);
        double best = 0; int best_lag = 0;
        for (int lag = 120; lag < 280; ++lag) {
            double sum = 0;
            for (unsigned i = 0; i + lag < N * 64; ++i) sum += samples[i] * samples[i + lag];
            if (sum > best) { best = sum; best_lag = lag; }
        }
        std::printf("string period %d samples (expected %.1f)\n", best_lag, 44100.0 / 220);
        check(best_lag >= 200 && best_lag <= 201, "string out of tune");
        // Damping lengthens the string's ring.
        Note dry(220, 0.5, 0.6, 0.2, 0.5, modes12, 0, 0.375), wet(220, 0.5, 0.6, 0.9, 0.5, modes12, 0, 0.375);
        dry.render(4); wet.render(4);
        dry.render(30); wet.render(30);
        check(wet.render(10) > dry.render(10), "string Damping direction");
    }
    {   // Short decay falls silent instead of freezing at a floor.
        Note note(440, 0.5, 0.5, 0, 0, modes12, 0);
        note.render(4);
        for (unsigned b = 0; b < 345; ++b) note.render(1);  // half a second
        check(note.render(20) < 1e-5, "a dead note still sounds");
    }
    {   // A voice that never started is silent.
        Note note(440, 0.5, 0.5, 0.6, 0, modes12, 0);
        note.voice.pending = 0;
        check(note.render(10) == 0, "unstruck voice sounds");
    }
    if (!failures) std::puts("mi_rings sound checks passed");
    return failures ? 1 : 0;
}
