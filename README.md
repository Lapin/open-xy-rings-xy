# MI Rings

Mutable Instruments Rings' four resonator models for the OP-XY, rewritten in
fixed point. Model chooses them as the module's button does. Modal: each note
strikes a bank of up to 24 two-pole resonators tuned to a partial series that
Structure bends from compressed, through harmonic, to stretched and bell-like.
String: a plucked Karplus-Strong loop with Rings' damping filters, dispersion
from Structure and a curved bridge. Sympathetic: the plucked string drives three
more strings tuned by Structure along Rings' ladder of octaves and fifths, which
swell and ring on after it. Chords: the same, with the three strings quantized to
one of Rings' eleven chords by Structure. In every model Bright sets the
excitation's colour and how quickly the highs fade, Damping the decay, Position
the strike or pluck point.

The partial layout, the string loop and the decay rules follow Rings'
`resonator.cc`, `string.cc` and `part.cc` (MIT, Emilie Gillet); the filters,
the exciters and the limiter are reimplemented in Q31 for the Blackfin, which has
no floating point. Rings' reverb model and its Easter eggs are not ported.

Two pages of four encoders; a press of any encoder switches pages.

| Control | What it does |
| --- | --- |
| Struct | Modal: the partial series, 0 compressed, about 25 harmonic, higher a bell. String: dispersion, harmonic at 25, stiff above. Sympathetic: the three strings' notes up the ladder. Chords: which chord |
| Bright | The exciter's low-pass, from a sixteenth of the note to sixteen times it, and how fast the highs fade |
| Damping | The decay, from a short knock to tens of seconds; 95 and above sustains a string for ever |
| Position | The strike or pluck point: 0 rings every partial, 100 the middle. On a string it also sets the pickup's comb and the pluck's length |
| Model (page 2, with Struct) | modal, string, sympath, chords |
| Strike (page 2, with Bright) | Modal only: 0 is a single pulse, as Rings strikes; higher turns it into a noise burst of up to 256 samples at the same energy |
| Modes (page 2, with Position) | Modal only: how many partials ring: bare 6, lean 12, full 18, lush 24, as Rings' polyphony button trades voices for partials; fewer cost less CPU |

The screen shows both pages in the shared header. Either side sit the Rings
panel's LEDs: the four models on the left (rings, bars, waves, a chord stack),
the four Modes steps on the right (one to four dots), the chosen one lit and
haloed for a second when it changes. In the middle is the body: a circle with
a dot at its centre, holding three inner rings for the modal body or the
strings of the string bodies, lit by Bright. Around it turns a mandala, a
different one for each model, lifted from Mutable's panel files into
straight-line outlines: the modal body has the Rings panel's Frequency knob,
twelve paisleys under two wavy rings; the string body has a lotus drawn in
code, eight petals with smaller petals and eyes inside, eight more between
them with beads and dots, in two rings; the sympathetic body has a drawn
ring of twelve interlocking circles, then small circles with triangles and
dots between them, in rings; the chords body has Rings' Structure knob, four nested
triangles under beaded arcs, with flowers between. The body circle shrinks to
sit inside the denser ones. The mandala turns slowly at rest, is kicked by
every note and eases back over Damping's time; Structure brings its pieces in
from the smallest out, and every note lights them in that order, a ripple from
the centre out, each recent note with its own. Position places the pluck point
on the body's diameter and a note sends ripples out from it. Reduced motion
holds the mandala still.

## Implementation

Each mode is a magic-circle resonator (`x += k y; y -= k x`, `k = 2 sin(pi f)`),
which keeps its frequency exact at low pitches and never drifts in amplitude on
its own. Its loss is applied every sample when the decay is shorter than about
a third of a second, and once per block with an exact fraction carry when it is
longer, so quiet tails keep decaying instead of freezing where a per-sample
loss would truncate to nothing. Modes above 0.49 of the sample rate are silent
and, as in Rings, not counted. The strike is Rings' pulse through a Q 1.5
low-pass placed by Bright, injected as filter state so its gain never needs a
large intermediate. Rings' 1.4-in, 0.8-out peak limiter follows the sum.

The string models are in `strings.h`: a 2048-sample loop read with Hermite
interpolation, Rings' damping FIR and a Q 0.5 low-pass placed by Damping and
Bright, with the loop shortened by that low-pass's phase delay at the note so
the string stays within a cent across the keyboard. String adds the dispersion
all-pass, the noise and bridge wobble of the loop length, and the DC block that
Structure fades in below a quarter. The pluck is Rings' Plucker: a noise burst
as long as Position's fraction of a period, through a comb and a low-pass eight
times the strike filter's cutoff. The sympathetic strings glide to their notes,
are dulled twice by Bright, damped at 0.7 plus 0.27 of Damping and more up the
bank, with the pickup wandering under a slow cosine, and are fed a fifth of the
main string's bridge output shared four ways, as Rings does with the internal
exciter. The loop is held to eight units of headroom so an infinite sustain
cannot overflow the integer.

A mode that has rung down below -120 dB is skipped until the next strike, so
a note costs less as it fades. A string voice goes quiet the same way once its
pluck is spent and the sum has fallen below the floor. Against a transcription of Rings' float
`ComputeFilters` over a grid of controls and notes, the port's partials sit
within 0.34 cents and their decay rates within 0.45 percent, with the same
partials counted in and out of the band.

The two-page header, the LEDs, the body and the mandala are drawn by
`display.cpp`; the four mandalas are 221 nodes turned about the body's
centre, one model's set shown at a time. `tools/extract_motif.py` lifts the
motifs from Mutable's `rings.ai` (converted with
`pdftocairo -svg`) into `tools/motif.json`, which `tools/gen_panel.py` scales
into `panel.svg.fragment`, draws the string and sympathetic sets itself, and lays
every set out in `mandala.h` for the display.
`tools/gen_panel.py` generates `panel.svg.fragment`.
`tools/gen_tables.py` generates `tables.h` (sine, Rings' stiffness and
damping curves, fractional octaves).

| File | Owns |
| --- | --- |
| [definition.json](definition.json) | Controls, presets, resource declarations and the display's budget |
| [core.h](core.h), [engine.cpp](engine.cpp) | Voice state, the model switch, the strike, the mode bank and the limiter |
| [strings.h](strings.h) | The string loop, the pluck and the sympathetic strings' notes |
| [series.h](series.h), [tables.h](tables.h), [tools/gen_tables.py](tools/gen_tables.py) | The partial series and the generated tables |
| [display.h](display.h), [display.cpp](display.cpp), [panel.svg.fragment](panel.svg.fragment) | The header, the LEDs, the body and the mandala |
| [tests/sound.cpp](tests/sound.cpp) | Sound checks: strike and pluck level and decay, modal and string tuning, each control's direction, every corner of every model bounded, a ten-second sympathetic sustain, silence |
| [machine.json](machine.json) | Kind, identity, description and builder glyph |

## Develop

Keep the [SDK](https://github.com/etcetc000/open-xy) beside this folder on
`staging`, or run the commands from an SDK checkout with `--machine` pointing here.

```sh
python ../open-xy/sdk.py test --machine .
python ../open-xy/sdk.py preview --machine . --out ../previews/mi_rings-001
python ../open-xy/tools/serve_harness.py --preview ../previews/mi_rings-001
```

The host CPU estimate overstates this machine: the estimate's reference loop
has no multiplies, so every Q31 multiply counts as the dozen host instructions
the portable helper takes rather than the Blackfin's one. The hand count is
about 10 instructions per mode per sample for a long-decay mode and 14 for a
short one, so 16 modes sit near 10,000 to 14,000 Blackfin instructions per
voice block and 24 near 15,000 to 21,000. A string is about 30 per sample, so
the single string sits near 2,500 and the four of the sympathetic models near
8,000, before the hardware measurement that the catalog requires. The voice
holds four 2048-sample loops, so the machine declares eight voices. Turn Modes
down on busy projects until it is measured.

## Hardware and publication

Follow the SDK's [firmware build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md)
and [hardware profiling workflow](https://github.com/etcetc000/open-xy/blob/staging/docs/profiling.md).
Hardware validation is pending: no device measurement has been made yet.
Keep firmware and stock content outside every checkout.

The code in this folder is covered by [LICENSE](LICENSE). The partial series,
string loop, decay rules and exciters follow Mutable Instruments Rings,
Copyright 2015 Emilie Gillet, MIT licence; no Rings source is included. The
screen's traced mandalas come from the Rings panel artwork in
Mutable's hardware files, CC BY-SA 3.0, Emilie Gillet; `tools/motif.json`
carries that licence.
