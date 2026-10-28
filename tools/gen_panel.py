#!/usr/bin/env python3
"""Generates panel.svg.fragment: the two-page header block, the Rings-style
model and modes LED rings with their icons either side, and the nodes the
display animates: the body circle with its inner rings and centre, the four mandalas that turn around the
body, the lit LEDs and their halos. Writes mandala.h beside the fragment.

Only forms seen on hardware: rect, circle, path of M/L/Z, text; at most two
decimals; explicit paint on every leaf. The body's nodes are parked off screen;
the mandalas' many pieces carry no authored transform, to fit the artwork in
the 64 KB the SDK allows, and the first frame places or hides every one.
"""
import json
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panel_paths import simplify  # noqa: E402

ID = 'mi_rings'
CX, CY = 240, 130                      # the body's centre
BODY = 40                              # the body circle's radius
INNER_RINGS = 6                        # the modal body's rings, as many as Position asks for
PATHS, POINTS = 4, 64                  # mutable outlines: the string, sympathetic and chords bodies' insides
LED_X = (30, 450)                      # model LEDs left, modes LEDs right
LED_Y = (82, 114, 146, 178)
LED_R = 8
PALETTE = 16
BARS = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
FONT = 'font-family="UniversNextPro-Light"'
ICON = '#8A8A8A'
PARKED = 'transform="matrix(1 0 0 1 640 320)"'
REVERSE = 2                            # a piece's fill flag: bit 0 filled, bit 1 turned the other way

# Node map, shared with display.h; the mandalas' nodes are laid out below and
# written to mandala.h for the display. The mutable outlines come first, as the
# contract requires, then the header's eight bars.
HEADER = PATHS                         # 4..11
BODY_NODE = HEADER + 8                 # 12
FIRST_INNER = BODY_NODE + 1            # 13..18
CENTRE = FIRST_INNER + INNER_RINGS     # 19
FIRST_MANDALA = CENTRE + 1             # 20
REACH = 88                             # a mandala's outermost pixel from the centre
TOLERANCE = 0.3                        # px: the outlines simplified to what the screen can show

# The modal body's motif is lifted from Mutable's Rings panel
# (tools/extract_motif.py); the other three bodies' mandalas are drawn here. The
# paisley is flipped so its tip points outwards, scaled, and slid out to span
# PAISLEY_R, with its eye and pupil as circles and a drawn drop, under two wavy
# rings redrawn with twelve waves so every tip sits in a notch. The traced
# others are scaled whole so their reach is REACH, and the body circle shrinks
# to sit inside them.
MOTIF = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'motif.json')))
MODELS = ('modal', 'string', 'sympathetic', 'chords')
PAISLEY_R = (44, 68)                   # the paisley's radial span, px
RING_R, RING_WAVE, RING_GAP = 73.5, 3.5, 4   # the wavy rings: mean radius, wave height, spacing
WAVES, WAVE_POINTS = 12, 144

_outline = [g for g in MOTIF['modal']['groups'] if g['name'] == 'outline'][0]['points']
_outline_y = [y for _, y in _outline]
_flip = min(_outline_y) + max(_outline_y)                       # y -> _flip - y mirrors the paisley radially
_scale = (PAISLEY_R[1] - PAISLEY_R[0]) / (max(_outline_y) - min(_outline_y))
_slide = PAISLEY_R[0] + max(_outline_y) * _scale                # outwards, so the base lands at PAISLEY_R[0]


def paisley(points):
    """A paisley piece in screen pixels, tip outwards (up), base at PAISLEY_R[0]."""
    return [(x * _scale, (_flip - y) * _scale - _slide) for x, y in points]


def paisley_circle_of(circle):
    """A circle of the motif (centre, radius in panel units) placed like the paisley."""
    cx, cy, r = circle
    (sx, sy), = paisley([(cx, cy)])
    return sx, sy, r * _scale


def teardrop(cx, cy, r, tip_y, steps=14):
    """A smooth drop: a circle of radius r about (cx, cy) drawn out to a tip at (cx, tip_y)."""
    d = cy - tip_y
    alpha = math.asin(r / d)                       # the tangents' angle from the axis
    start, end = -math.pi / 2 + alpha, 3 * math.pi / 2 - alpha
    arc = [(cx + r * math.cos(start + (end - start) * i / steps), cy + r * math.sin(start + (end - start) * i / steps))
           for i in range(steps + 1)]
    return [(cx, tip_y)] + arc


def wavy_ring(radius):
    return [((radius + RING_WAVE * math.cos(WAVES * t)) * math.sin(t), -(radius + RING_WAVE * math.cos(WAVES * t)) * math.cos(t))
            for t in (2 * math.pi * i / WAVE_POINTS for i in range(WAVE_POINTS))]


def petal(r0, r1, width, turn=0.0, point=0.6, steps=12):
    """A lotus petal from radius r0 to a tip at r1, `width` across at its widest,
    rounded at the base and pointed at the tip, authored upright then turned."""
    pts = []
    for side in (1, -1):
        rng = range(steps + 1) if side == 1 else range(steps - 1, 0, -1)
        for i in rng:
            t = i / steps
            w = width / 2 * math.sin(math.pi * t) ** point * (1 - (0.35 * t if point < 1 else 0))
            pts.append((side * w, -(r0 + (r1 - r0) * t)))
    a = math.radians(turn)
    c, s = math.cos(a), math.sin(a)
    return [(x * c - y * s, x * s + y * c) for x, y in pts]


def turned(cx, cy, turn):
    a = math.radians(turn)
    return cx * math.cos(a) - cy * math.sin(a), cx * math.sin(a) + cy * math.cos(a)


def drawn_lotus():
    """The string body's mandala, drawn rather than traced: a ring with a wave
    around it, eight petals with smaller petals and eyes inside, eight more
    petals between them, an outer ring and its dots. Pieces are (name, shape, fill, layer, count)."""
    fold = 8
    pieces = [('ring_inner', ('path', circle_path(45)), False, 0, 1),
              ('wave', ('path', wave_ring(48.5, 2 * fold, 2.5)), REVERSE, 0, 1),
              ('petal', ('path', petal(53, 74, 19)), False, 1, fold),
              ('petal_inner', ('path', petal(56, 67, 9)), False, 2, fold),
              ('eye', ('circle', 0, -60, 2), True, 3, fold),
              ('petal_outer', ('path', petal(54, 86, 24, 360 / fold / 2)), False, 4, fold),
              ('ring_outer', ('path', circle_path(87)), False, 5, 1),
              ('bead_ring', ('circle', *turned(0, -67, 360 / fold / 2), 6), False, 5, fold),
              ('bead', ('circle', *turned(0, -67, 360 / fold / 2), 4), True, 5, fold),
              ('dot', ('circle', *turned(0, -79, 360 / fold / 2), 2), True, 6, fold)]
    return fold, pieces, 1.0


def triangle(r0, r1, half_degrees, turn):
    """An isosceles triangle on the arc at r0, pointing out to r1, turned."""
    a = math.radians(half_degrees)
    pts = [(0, -r1), (r0 * math.sin(a), -r0 * math.cos(a)), (-r0 * math.sin(a), -r0 * math.cos(a))]
    return [turned(x, y, turn) for x, y in pts]


def drawn_sympathetic():
    """The sympathetic body's mandala, drawn: twelve circles each with a
    leaf's veins inside, a ring, small circles with triangles between them,
    and an outer ring; the circles and their veins turn the other way."""
    fold = 12
    half = 360 / fold / 2
    pieces = [('loop', ('circle', 0, -56, 14), REVERSE, 0, fold),
              ('veins', ('path', leaf_veins(0, -56, 14)), REVERSE, 1, 1 * fold),
              ('ring_mid', ('path', circle_path(71)), False, 2, 1),
              ('small', ('circle', 0, -76, 3), False, 3, fold),
              ('triangle', ('path', triangle(73, 85, 8, half)), False, 4, fold),
              ('ring_outer', ('path', circle_path(87)), False, 5, 1)]
    return fold, pieces, 1.0


def tower(r_base, r_top, r_tip, w, turn, rungs, steps=10, half=0.4):
    """A pointed window: walls from r_base up to r_top, sides bowing out to a tip
    at r_tip, with rungs across the arch drawn as thin detours of the outline."""
    control = (w * 1.15, -(r_top + (r_tip - r_top) * 0.5))
    def right(t):                                            # the right side, wall top to tip
        return ((1 - t) ** 2 * w + 2 * (1 - t) * t * control[0],
                (1 - t) ** 2 * -r_top + 2 * (1 - t) * t * control[1] + t * t * -r_tip)
    fine = [right(i / 400) for i in range(401)]
    def x_at(y):                                             # the side's x at height y (y negative)
        return min(fine, key=lambda p: abs(p[1] - y))[0]
    pts = [(-w, -r_base), (-w, -r_top)]
    pts += [(-x, y) for x, y in (right(i / steps) for i in range(1, steps))]
    pts.append((0, -r_tip))
    down = [right(i / steps) for i in range(steps - 1, 0, -1)]
    pending = sorted(rungs)                                  # outermost first, since y grows down the side
    pending = sorted(rungs, reverse=True)
    for x, y in down:
        while pending and -pending[0] <= y:                  # passed a rung's height: detour across
            r = pending.pop(0)
            xr = x_at(-r)
            pts += [(xr, -r), (-xr + half, -r), (-xr + half, -r + 2 * half), (xr, -r + 2 * half)]
        pts.append((x, y))
    pts += [(w, -r_top), (w, -r_base)]
    return [turned(x, y, turn) for x, y in pts]


def veined(r0, r1, width, turn=0.0, half=0.4):
    """A lens petal with a vein up its middle, as one outline."""
    pts = petal(r0, r1, width, 0, 1.0)
    pts += [(half, -(r0 + 2.5)), (half, -(r1 - 3)), (-half, -(r1 - 3)), (-half, -(r0 + 2.5))]
    return [turned(x, y, turn) for x, y in pts]


def crown(r0, r1, fingers, fill=0.55, tip_steps=6):
    """A fringe of rounded fingers from r0 to r1 as one outline, joined along the arc at r0."""
    pts = []
    pitch = 2 * math.pi / fingers
    half = fill * pitch / 2                                  # a finger's half width, radians at r0
    w = r0 * math.sin(half)                                  # ... and in pixels
    for k in range(fingers):
        a = k * pitch
        pts.append(turned(-w, -r0, math.degrees(a)))
        centre = r1 - w                                      # the tip's semicircle
        for i in range(tip_steps + 1):
            t = math.pi * i / tip_steps
            pts.append(turned(-w * math.cos(t), -(centre + w * math.sin(t)), math.degrees(a)))
        pts.append(turned(w, -r0, math.degrees(a)))
    return pts


def drawn_chords():
    """The chords body's mandala, drawn as a layered flower: three rings of
    lens petals, each turned half a step from the last, the innermost raised
    towards the big ones' tips, the second veined under pointed windows with
    rungs, rings between, a ring and a fringe of rounded fingers."""
    fold = 8
    half = 360 / fold / 2
    pieces = [('petal_a', ('path', petal(50, 64, 7, 0, 1.0)), False, 0, fold),
              ('petal_b', ('path', veined(42, 56, 10, half, 0.25)), False, 1, fold),
              ('petal_c', ('path', petal(44, 70, 15, 0, 1.0)), False, 2, fold),
              ('tower', ('path', tower(42, 57, 72, 7, half, (60, 65), half=0.25)), False, 3, fold),
              ('dot', ('circle', *turned(0, -71, half / 2), 3), False, 4, 2 * fold),
              ('ring', ('path', circle_path(77)), False, 5, 1),
              ('fringe', ('path', crown(77.5, 88, 40)), REVERSE, 6, 1)]
    return fold, pieces, 1.0


def leaf_veins(cx, cy, radius, veins=(7, 1.5, -4), angle=42, half=0.4):
    """A leaf's midrib along the radius with veins each side, as one thin
    closed ribbon inside a circle: up the midrib drawing the right veins, down
    it drawing the left. The midrib spans the circle; the veins stop just
    inside its edge, so their stroke never crosses it."""
    a = math.radians(angle)
    inside = radius - 1.2                                 # the veins stop short of the edge
    def to_edge(y, side):
        dx, dy = side * math.sin(a), -math.cos(a)        # towards the tip
        t = -y * dy + math.sqrt((y * dy) ** 2 - (y * y - inside * inside))
        return dx * t, y + dy * t
    def vein(y, side):
        ex, ey = to_edge(y, side)
        nx, ny = -side * math.cos(a) * half, -math.sin(a) * half   # a step across the vein
        return [(0, y), (ex, ey), (ex + nx, ey + ny), (nx, y + ny)]
    base, tip = radius, -radius                           # the midrib spans the circle; the tip points outwards (-y)
    pts = [(half, base)]
    for y in sorted(veins, reverse=True):
        pts += vein(y, 1)
    pts += [(half, tip), (-half, tip)]
    for y in sorted(veins):
        pts += [(x - 2 * half, yy) for x, yy in vein(y, -1)[::-1]]
    pts += [(-half, base)]
    return [(cx + x, cy + y) for x, y in pts]


def wave_ring(radius, waves, height, steps=192):
    """A ring with `waves` crests, the troughs at angle 0 and every 360/waves."""
    return [((radius - height * math.cos(waves * t)) * math.sin(t), -(radius - height * math.cos(waves * t)) * math.cos(t))
            for t in (2 * math.pi * i / steps for i in range(steps))]


def circle_path(radius, steps=72):
    return [(radius * math.sin(2 * math.pi * i / steps), -radius * math.cos(2 * math.pi * i / steps)) for i in range(steps)]


def reach(groups):
    """The radial span of a motif's pieces, in its own units."""
    low, high = 1e9, 0
    for grp in groups:
        if 'circle' in grp:
            cx, cy, r = grp['circle']
            d = math.hypot(cx, cy)
            low, high = min(low, abs(d - r)), max(high, d + r)
        else:
            for x, y in grp['points']:
                d = math.hypot(x, y)
                low, high = min(low, d), max(high, d)
    return low, high


def num(v, decimals=2):
    s = f'{v:.{decimals}f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def poly(points, stroke, width=1, fill='none', attrs='', decimals=2):
    d = 'M' + ' L'.join(f'{num(x, decimals)},{num(y, decimals)}' for x, y in points) + ' Z'
    return f'<path{attrs} d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'


def node(i, points, stroke='#808080', width=1, decimals=2):
    return poly(points, stroke, width, attrs=f' id="{ID}_node_{i}" {PARKED}', decimals=decimals)


def circle_node(i, r, fill='none', stroke='#808080', width=1):
    return f'<circle id="{ID}_node_{i}" {PARKED} cx="0" cy="0" r="{num(r)}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'


def rect(cx, cy, w, h, colour):
    return poly([(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2), (cx - w / 2, cy + h / 2)],
                colour, 0.5, fill=colour)


def dot(cx, cy, r, colour):
    return f'<circle cx="{num(cx)}" cy="{num(cy)}" r="{num(r)}" fill="{colour}" stroke="{colour}" stroke-width="0.5"/>'


def ribbon(points, colour, half=0.4):
    """A polyline as a thin closed shape: out along the points, back a little to
    the right, so the OP-XY has area to draw and no streaks."""
    return poly(points + [(x + 2 * half, y) for x, y in points[::-1]], colour, 0.5, fill=colour)


def model_icon(k, cx, cy):
    if k == 0:   # modal: concentric rings
        return [f'<circle cx="{cx}" cy="{cy}" r="2.2" fill="none" stroke="{ICON}" stroke-width="0.8"/>',
                f'<circle cx="{cx}" cy="{cy}" r="4.6" fill="none" stroke="{ICON}" stroke-width="0.8"/>']
    if k == 1:   # string: a spectrum of bars
        return [rect(cx + x, cy, 1, h, ICON) for x, h in ((-4, 5), (-2, 8), (0, 6), (2, 9), (4, 4))]
    if k == 2:   # sympathetic: three wavy strings
        lines = []
        for x in (-3.6, -0.4, 2.8):
            lines.append(ribbon([(cx + x + 1.1 * math.sin(i * math.pi / 2), cy - 5 + i * 2.5) for i in range(5)], ICON))
        return lines
    # chords: a stack of bars
    return [rect(cx, cy + y, w, 1.2, ICON) for y, w in ((-3.5, 9), (0, 6), (3.5, 3.5))]


def modes_icon(k, cx, cy):
    spots = [[(0, 0)], [(-2.6, 0), (2.6, 0)], [(0, -2.6), (-2.4, 1.6), (2.4, 1.6)],
             [(-2.4, -2.4), (2.4, -2.4), (-2.4, 2.4), (2.4, 2.4)]][k]
    return [dot(cx + x, cy + y, 1.3, ICON) for x, y in spots]


out = [f'<g id="{ID}_panel">',
       '<!-- Two-page header in the proven hurdygurdy form: a black rect covers the stock header,',
       '     then the bar steps and both pages\' titles and values at their resting sizes. -->',
       '<rect x="0" y="0" width="480" height="220" fill="#000000"/>']
out.append('<!-- The mutable outlines: thin ribbons of exactly POINTS distinct points, reshaped every frame. -->')
for i in range(PATHS):
    flat = [(-31 + 2 * k, -0.4) for k in range(POINTS // 2)]
    out.append(node(i, flat + [(x, 0.4) for x, y in flat[::-1]], width=0.5, decimals=1))
for i, colour in enumerate(BARS):
    out.append(f'<path id="{ID}_node_{HEADER + i}" transform="matrix(1 0 0 1 {i * 60} 0)" d="M0,0 L60,0 L60,20 L0,20 Z" '
               f'fill="{colour}" stroke="{colour}" stroke-width="0.5"/>')
titles = ['struct', 'bright', 'damping', 'position', 'model', 'strike', 'damping', 'modes']
values = ['50', '50', '50', '50', 'modal', '0', '50', 'lean']
for i in range(4):
    dark = '#000000' if i >= 2 else '#CBCBCB'
    out.append(f'<text id="{ID}_text_{i}" transform="matrix(1 0 0 1 {5 + 120 * i} 10)" fill="{dark}" {FONT} font-size="10px">{titles[i]}</text>')
for i in range(4):
    dark = '#000000' if i >= 2 else '#CBCBCB'
    out.append(f'<text id="{ID}_text_{4 + i}" transform="matrix(1 0 0 1 {65 + 120 * i} 17.4)" fill="{dark}" {FONT} font-size="20px">{values[i]}</text>')
for i in range(4):
    out.append(f'<text id="{ID}_text_{8 + i}" transform="matrix(1 0 0 1 {5 + 120 * i} 33)" fill="#5A5A5A" {FONT} font-size="10px">{titles[4 + i]}</text>')
for i in range(4):
    out.append(f'<text id="{ID}_text_{12 + i}" transform="matrix(1 0 0 1 {65 + 120 * i} 33)" fill="#707070" {FONT} font-size="13px">{values[4 + i]}</text>')

out.append('<!-- The LEDs\' rings and icons, as on the Rings panel: models on the left, modes on the right. -->')
for k, y in enumerate(LED_Y):
    for x in LED_X:
        out.append(f'<circle cx="{x}" cy="{y}" r="{LED_R}" fill="none" stroke="{BARS[2]}" stroke-width="1.2"/>')
    out += model_icon(k, LED_X[0], y)
    out += modes_icon(k, LED_X[1], y)

out.append('<!-- Everything below is placed by the display. The body: its circle, the modal rings (authored at the body radius, scaled down) and centre. -->')
out.append(circle_node(BODY_NODE, BODY, stroke='#A0A0A0'))
for i in range(INNER_RINGS):
    out.append(circle_node(FIRST_INNER + i, 40))
out.append(f'<circle id="{ID}_node_{CENTRE}" {PARKED} cx="0" cy="0" r="2" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="0.5"/>')
out.append('<!-- The mandalas, one per model, lifted from Mutable\'s panels: every piece authored upright')
out.append('     at the centre and turned by the display, which shows one model\'s set at a time. -->')
table = []          # per model: (fold, body scale Q10, [(first, count, layer, fill)])
next_node = FIRST_MANDALA
for model in MODELS:
    spec = MOTIF.get(model, {})
    fold = spec.get('fold', 0)
    groups = []
    if model == 'modal':
        P = {grp['name']: grp for grp in spec['groups']}
        ex, ey, er = paisley_circle_of(P['eye']['circle'])
        er *= 0.8
        drop = teardrop(ex, ey - er - 4.0, 1.3, ey - er - 8.6)
        pieces = [('pupil', ('circle', ex, ey, 1.2), True, 0, fold), ('eye', ('circle', ex, ey, er), False, 1, fold),
                  ('drop', ('path', drop), False, 2, fold), ('inner', ('path', paisley(P['inner']['points'])), False, 3, fold),
                  ('outline', ('path', paisley(P['outline']['points'])), False, 4, fold),
                  ('ring_inner', ('path', wavy_ring(RING_R)), False, 5, 1), ('ring_outer', ('path', wavy_ring(RING_R + RING_GAP)), False, 5, 1)]
        body = 1.0
    elif model == 'string':
        fold, pieces, body = drawn_lotus()
    elif model == 'sympathetic':
        fold, pieces, body = drawn_sympathetic()
    elif model == 'chords':
        fold, pieces, body = drawn_chords()
    else:
        low, high = reach(spec['groups'] + spec.get('single', []))
        k = REACH / high
        body = min(1.0, (low * k - 4) / BODY)
        def scaled_piece(grp):
            if 'circle' in grp:
                cx, cy, r = grp['circle']
                return ('circle', cx * k, cy * k, r * k)
            return ('path', [(x * k, y * k) for x, y in grp['points']])
        pieces = [(grp['name'], scaled_piece(grp), grp['fill'], grp['layer'], fold) for grp in spec['groups']]
        pieces += [(grp['name'], scaled_piece(grp), grp['fill'], grp['layer'], 1) for grp in spec.get('single', [])]
    out.append(f'<!-- {model}: {fold}-fold -->')
    for name, shape, fill, layer, count in pieces:
        if shape[0] == 'path':
            xs, ys = [x for x, _ in shape[1]], [y for _, y in shape[1]]
            extent = max(max(xs) - min(xs), max(ys) - min(ys))
            shape = ('path', simplify(shape[1], min(TOLERANCE, extent / 24)))   # small pieces keep their curves
        groups.append((next_node, count, layer, fill))
        for i in range(count):
            n = next_node + i
            paint = ('#808080', '#808080') if int(fill) & 1 else ('none', '#808080')
            if shape[0] == 'circle':
                _, cx, cy, r = shape
                out.append(f'<circle id="{ID}_node_{n}" cx="{num(cx, 1)}" cy="{num(cy, 1)}" r="{num(r, 1)}" fill="{paint[0]}" stroke="{paint[1]}" stroke-width="{".5" if int(fill) & 1 else ".8"}"/>')
            else:
                out.append(poly(shape[1], '#808080', '.8', fill=paint[0], attrs=f' id="{ID}_node_{n}"', decimals=1))
        next_node += count
    table.append((fold, round(body * 1024), groups))
MODEL_LED = next_node                  # the lit model LED; the unlit rings are artwork
MODES_LED = next_node + 1              # the lit modes LED
HALO = next_node + 2                   # model, modes
NODES = HALO + 2
assert NODES <= 256, NODES

header = ['// Generated by tools/gen_panel.py from tools/motif.json: the mandalas\' nodes.',
          '#pragma once', '#include <opxy/types.hpp>', '', 'namespace mi_rings {',
          f'const opxy::u32 kPaths = {PATHS}, kPathPoints = {POINTS}, kHeaderNode = {HEADER}, kBodyNode = {BODY_NODE};',
          f'const opxy::u32 kFirstInner = {FIRST_INNER}, kInnerRings = {INNER_RINGS}, kCentreNode = {CENTRE};',
          f'const opxy::u32 kFirstMandala = {FIRST_MANDALA}, kModelLed = {MODEL_LED}, kModesLed = {MODES_LED}, kHaloNode = {HALO}, kNodes = {NODES};',
          'const opxy::u32 kMandalaGroups = %d;' % max(len(t[2]) for t in table),
          'struct MandalaGroup { opxy::u16 first; opxy::u8 count, layer, fill; };   // fill: bit 0 filled, bit 1 turns the other way',
          'struct Mandala { opxy::u32 fold; opxy::i32 body_scale; MandalaGroup groups[%d]; };' % max(len(t[2]) for t in table),
          'const Mandala kMandalas[4] = {']
for (fold, body, groups), model in zip(table, MODELS):
    rows = ', '.join('{%d, %d, %d, %d}' % (f, c, l, int(fi)) for f, c, l, fi in groups)
    pad = ', '.join('{0, 0, 0, 0}' for _ in range(max(len(t[2]) for t in table) - len(groups)))
    header.append(f'    {{{fold}, {body}, {{{rows}{", " + pad if pad else ""}}}}},  // {model}')
header += ['};', '}', '']
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mandala.h'), 'w').write('\n'.join(header))
definition = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'definition.json')
text = open(definition).read()
text = re.sub(r'"paths": \d+', f'"paths": {PATHS}', text, count=1)
text = re.sub(r'"points": \d+', f'"points": {POINTS}', text, count=1)
open(definition, 'w').write(re.sub(r'"nodes": \d+', f'"nodes": {NODES}', text, count=1))

out.append('<!-- The lit LED of each column, and the halo that marks a change. -->')
out.append(circle_node(MODEL_LED, LED_R, stroke='#808080', width=1.2))
out.append(circle_node(MODES_LED, LED_R, stroke='#808080', width=1.2))
for i in range(2):
    out.append(circle_node(HALO + i, LED_R + 4, stroke='#808080', width=1))
for i in range(PALETTE):
    grey = f'#{i * 17:02x}{i * 17:02x}{i * 17:02x}'
    out.append(f'<circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{grey}" stroke="{grey}" stroke-width="1"/>')
out.append('</g>')
print('\n'.join(out))
