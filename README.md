# MI Rings

A modal resonator for the OP-XY after the modal mode of Mutable Instruments
Rings, rewritten in fixed point. Each note strikes a bank of up to 24 two-pole
resonators tuned to a partial series. Structure bends the series from compressed,
through harmonic, to stretched and bell-like; Bright sets the strike's colour and
how quickly the upper partials fade; Damping sets the decay; Position moves the
strike point along the string, thinning every second partial towards the middle.

The partial layout and the decay rules follow Rings' `resonator.cc` and
`part.cc` (MIT, Emilie Gillet); the filters, the strike and the limiter are
reimplemented in Q31 for the Blackfin, which has no floating point. The sound
and the strings' sympathetic models are not ported.

Two pages of four encoders; a press of any encoder switches pages.

| Control | What it does |
| --- | --- |
| Struct | The partial series: 0 compresses it, about 25 is harmonic, higher stretches it towards a bell |
| Bright | The strike's low-pass, from a sixteenth of the note to sixteen times it, and how fast high partials fade |
| Damping | The decay, from a short knock to tens of seconds |
| Position | The strike point: 0 rings every partial, 100 strikes the middle and mutes every second one |
| Modes (page 2, with Struct) | How many partials ring, 4 to 24; fewer cost less CPU |
| Strike (page 2, with Bright) | 0 is a single pulse, as Rings strikes; higher turns it into a noise burst of up to 256 samples at the same energy |

The screen shows both pages in the shared header. Under it, a square tile of
concentric rings after the Rings logo, one ring per partial at 12 pixels an
octave above the note: Structure bunches them into a harmonic series or spreads
them into a bell, Position sets the strike point along the tile and weights each
ring, and a note sends ripples out from the strike point for as long as Damping
lets them linger. Paisley filigree either side nods to the Mutable panels.

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

A mode that has rung down below -120 dB is skipped until the next strike, so
a note costs less as it fades. Against a transcription of Rings' float
`ComputeFilters` over a grid of controls and notes, the port's partials sit
within 0.34 cents and their decay rates within 0.45 percent, with the same
partials counted in and out of the band.

The two-page header and the ring tile are drawn by `display.cpp` from
`series.h`, which both the sound and the screen use to lay out the partials.
`tools/gen_panel.py` generates `panel.svg.fragment`.
`tools/gen_tables.py` generates `tables.h` (sine, Rings' stiffness and
damping curves, fractional octaves).

| File | Owns |
| --- | --- |
| [definition.json](definition.json) | Controls, presets, resource declarations and the display's budget |
| [core.h](core.h), [engine.cpp](engine.cpp) | Voice state, the strike, the mode bank and the limiter |
| [series.h](series.h), [tables.h](tables.h), [tools/gen_tables.py](tools/gen_tables.py) | The partial series and the generated tables |
| [display.h](display.h), [display.cpp](display.cpp), [panel.svg.fragment](panel.svg.fragment) | The header and the ring tile |
| [tests/sound.cpp](tests/sound.cpp) | Sound checks: strike level and decay, tuning, each control's direction, every corner bounded, silence |
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
voice block and 24 near 15,000 to 21,000, before the hardware measurement that
the catalog requires. Turn Modes down on busy projects until it is measured.

## Hardware and publication

Follow the SDK's [firmware build guide](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md)
and [hardware profiling workflow](https://github.com/etcetc000/open-xy/blob/staging/docs/profiling.md).
Hardware validation is pending: no device measurement has been made yet.
Keep firmware and stock content outside every checkout.

The code in this folder is covered by [LICENSE](LICENSE). The partial series,
decay rules and strike follow Mutable Instruments Rings, Copyright 2015 Emilie
Gillet, MIT licence; no Rings source is included.
