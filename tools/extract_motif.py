#!/usr/bin/env python3
"""Lifts one motif per model from Mutable Instruments' panel artwork
(hardware_design/panel/<module>.ai in github.com/pichenettes/eurorack,
CC BY-SA 3.0) into motif.json: polylines in panel points about the knob's
centre, each motif upright at angle 0, repeated `fold` times by the display.

    pdftocairo -svg rings.ai rings.svg  (and elements.ai, clouds.ai)
    python3 tools/extract_motif.py <folder with rings.svg, elements.svg and clouds.svg> > tools/motif.json

  modal        Rings, Frequency knob: a paisley, the wavy rings redrawn by gen_panel
  string       Clouds, Position knob: a lotus of four petals and four flowers in rings
  sympathetic  Elements, Flow knob: a spike flanked by two teardrops with eyes
  chords       Rings, Structure knob: nested rounded triangles under a beaded arc,
               with a flower, leaves and drops
"""
import json, math, os, re, sys
import panel_paths as pp

folder = sys.argv[1]
svgs = {name: open(os.path.join(folder, name + '.svg')).read() for name in ('rings', 'elements', 'clouds')}
paths = {name: [m.group(2) for m in re.finditer(r'<path([^>]*) d="([^"]*)"', svg)] for name, svg in svgs.items()}
CENTRES = {'frequency': (53.83, 94.62), 'structure': (147.06, 94.44), 'mallet': (229.09, 136.33), 'flow': (135.85, 135.85), 'position': (51.1, 115.0)}


def piece(panel, knob, index, sub, turn=0.0, tolerance=0.05):
    pts = pp.subpaths(paths[panel][index])[sub][1]
    cx, cy = CENTRES[knob]
    local = pp.simplify([(x - cx, y - cy) for x, y in pts], tolerance)
    a = math.radians(turn)
    c, s = math.cos(a), math.sin(a)
    return [(round(x * c - y * s, 3), round(x * s + y * c, 3)) for x, y in local]


def circle(points):
    cx = sum(x for x, _ in points) / len(points)
    cy = sum(y for _, y in points) / len(points)
    return [round(cx, 3), round(cy, 3), round(sum(math.hypot(x - cx, y - cy) for x, y in points) / len(points), 3)]


def group(name, points=None, circle_of=None, fill=False, layer=0):
    g = {'name': name, 'layer': layer, 'fill': fill}
    if circle_of is not None: g['circle'] = circle(circle_of)
    else: g['points'] = points
    return g


T = 10.0  # the paisley at 350 degrees, turned to upright
motif = {
    'source': 'Mutable Instruments panel artwork: Rings (Frequency, Structure), Elements (Flow) and Clouds (Position), CC BY-SA 3.0',
    'units': 'panel points about the knob centre; y down; each motif upright at angle 0',
    'modal': {'fold': 12, 'groups': [
        group('pupil', circle_of=piece('rings', 'frequency', 114, 0, T), fill=True, layer=0),
        group('eye', circle_of=piece('rings', 'frequency', 94, 1, T), layer=1),
        group('inner', piece('rings', 'frequency', 60, 10, T, 0.012), layer=3),
        group('outline', piece('rings', 'frequency', 60, 11, T, 0.03), layer=4),
    ]},
    'string': {'fold': 4, 'groups': [
        group('pupil', circle_of=piece('clouds', 'position', 129, 0), fill=True, layer=0),
        group('eye', circle_of=piece('clouds', 'position', 129, 1), layer=1),
        group('petal_inner', piece('clouds', 'position', 128, 0, 0, 0.02), layer=2),
        group('petal', piece('clouds', 'position', 111, 0, 0, 0.02), layer=3),
        group('flower_dot', circle_of=piece('clouds', 'position', 130, 0), fill=True, layer=0),
        group('flower', piece('clouds', 'position', 112, 9, 0, 0.015), layer=2),
        group('flower_drop', piece('clouds', 'position', 16, 3, 0, 0.02), layer=3),
    ], 'single': [
        group('ring', circle_of=piece('clouds', 'position', 16, 5), layer=4),
    ]},
    'sympathetic': {'fold': 10, 'groups': [
        group('eye_left', circle_of=piece('elements', 'flow', 476, 0), layer=0),
        group('eye_right', circle_of=piece('elements', 'flow', 487, 0), layer=0),
        group('drop_left', piece('elements', 'flow', 436, 1), layer=1),
        group('drop_right', piece('elements', 'flow', 447, 1), layer=1),
        group('spike', piece('elements', 'flow', 332, 1), layer=3),
    ]},
    'chords': {'fold': 4, 'groups': [
        group('triangle_core', piece('rings', 'structure', 192, 1), layer=0),
        group('triangle_mid', piece('rings', 'structure', 140, 1), layer=1),
        group('triangle', piece('rings', 'structure', 116, 136), layer=2),
        group('flower_dot', circle_of=piece('rings', 'structure', 196, 0), fill=True, layer=0),
        group('flower_ring', circle_of=piece('rings', 'structure', 148, 1), layer=1),
        group('flower', piece('rings', 'structure', 116, 124), layer=2),
        group('drop_left', piece('rings', 'structure', 116, 68, 0, 0.015), layer=3),
        group('drop_right', piece('rings', 'structure', 116, 103, 0, 0.015), layer=3),
        group('leaf_left', piece('rings', 'structure', 116, 113, 0, 0.015), layer=4),
        group('leaf_right', piece('rings', 'structure', 116, 115, 0, 0.015), layer=4),
        group('arc', piece('rings', 'structure', 116, 130, 0, 0.04), layer=5),
    ]},
}
json.dump(motif, sys.stdout, separators=(',', ':'))
