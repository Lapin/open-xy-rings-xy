# mi_rings: hardware CPU measurement

Machine commit: `fdfc63726f708b9d2e3e3401449fafdee43c7259`. Benchmark: `opxy-synth-c8-v1`. Scope: whole voice, including oscillator/filter.

Median of 3 run means: **1.407%** of the stock 580,352-cycle block budget. Run means: 1.418%, 1.407%, 1.372%.

This is a synthetic measurement on the USB-MIDI command endpoint, not total device CPU, maximum polyphony or a worst-case settings sweep. Graphics are excluded. Callback tests exclude sample preparation, allocation, stock mixing and voice processing; they include stopwatch-call overhead and may include interrupt time. See docs/profiling.md for the versioned workloads.

Firmware SHA-256: `ae64ade355e90b806f01b726d3836f5e450a768fad54592f91debf864116b3f1`.

Compile report SHA-256: `b07b330087de9c5ac01d26b2dd61b10867fca2b1f089fbb347fb4ee5d9b1bb1a`.

Attribution uses the user-confirmed installed package, expected case order and exact report count; the device does not attest its firmware hash. Raw captures remain private because they contain device identity.
