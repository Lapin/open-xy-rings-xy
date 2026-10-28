# mi_rings: hardware CPU measurement

Machine commit: `a97ba6f36f3bff613bec7e4497039d2cb14f32a8`. Benchmark: `opxy-synth-c8-v1`. Scope: whole voice, including oscillator/filter.

Median of 3 run means: **1.408%** of the stock 580,352-cycle block budget. Run means: 1.419%, 1.408%, 1.372%.

This is a synthetic measurement on the USB-MIDI command endpoint, not total device CPU, maximum polyphony or a worst-case settings sweep. Graphics are excluded. Callback tests exclude sample preparation, allocation, stock mixing and voice processing; they include stopwatch-call overhead and may include interrupt time. See docs/profiling.md for the versioned workloads.

Firmware SHA-256: `24ec09e4d4fd3c2b1d28380e2cab5a9e82133b16722d89a2b15f96a7adc613e7`.

Compile report SHA-256: `667994fb2635125ace18d122c5db81cc2307af5e4222ff4a65bb4d592320dda5`.

Attribution uses the user-confirmed installed package, expected case order and exact report count; the device does not attest its firmware hash. Raw captures remain private because they contain device identity.
