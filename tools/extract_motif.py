#!/usr/bin/env python3
"""Lifts the Frequency mandala's motif from Mutable Instruments' Rings panel
(rings/hardware_design/panel/rings.ai in github.com/pichenettes/eurorack,
CC BY-SA 3.0) into motif.json: one upright paisley, the two wavy rings and a
circled dot, as polylines in panel points about the knob's centre.

    pdftocairo -svg rings.ai rings.svg
    python3 tools/extract_motif.py rings.svg > tools/motif.json
"""
import json, math, re, sys
import panel_paths as pp

svg = open(sys.argv[1]).read()
paths = [m.group(2) for m in re.finditer(r'<path([^>]*) d="([^"]*)"', svg)]


def piece(index, sub, turn=0.0, tolerance=0.05):
    pts = pp.subpaths(paths[index])[sub][1]
    local = [(x - pp.CENTRE[0], y - pp.CENTRE[1]) for x, y in pts]
    local = pp.simplify(local, tolerance)
    a = math.radians(turn)
    c, s = math.cos(a), math.sin(a)
    return [(round(x * c - y * s, 3), round(x * s + y * c, 3)) for x, y in local]


# The paisley at 350 degrees, turned 10 degrees clockwise to upright.
TURN = 10.0
motif = {
    'source': 'Mutable Instruments Rings panel, Frequency mandala (CC BY-SA 3.0)',
    'units': 'panel points about the knob centre; y down; the paisley points up',
    'paisley': {
        'outline': piece(60, 11, TURN),
        'inner': piece(60, 10, TURN, 0.03),
        'eye': piece(94, 1, TURN),
        'pupil': piece(114, 0, TURN),
        'drop': piece(95, 1, TURN, 0.03),
        'beads': [piece(60, k, TURN, 0.03) for k in range(10)],
    },
    'ring_inner': piece(25, 0, 0, 0.08),
    'ring_outer': piece(6, 0, 0, 0.08),
    'dot_ring': piece(18, 1, 0),
    'dot': piece(37, 0, 0),
}
json.dump(motif, sys.stdout, separators=(',', ':'))
