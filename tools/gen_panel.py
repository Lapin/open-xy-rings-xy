#!/usr/bin/env python3
"""Generates panel.svg.fragment: the two-page header block, the Rings-style
model and modes LEDs with their icons either side, and the nodes the display
animates: the body circle with its inner rings and strings, the pluck point
and ripples, and the Rings panel's mandala of paisleys, wavy rings and dots
that turns around the body.

Only forms seen on hardware: rect, circle, path of M/L/Z, text; at most two
decimals; explicit paint on every leaf; bound nodes parked off screen.
"""
import json
import math
import os

ID = 'mi_rings'
CX, CY = 240, 130                      # the body's centre
BODY = 42                              # the body circle's radius
INNER = [12, 22, 32]                   # the inner rings
STRING_HALF = 38                       # half a string's authored length
PAISLEYS = 12
REACH = 87                             # the mandala's outermost pixel from the centre
LED_X = (30, 450)                      # model LEDs left, modes LEDs right
LED_Y = (82, 114, 146, 178)
LED_R = 8
PALETTE = 16
BARS = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
FONT = 'font-family="UniversNextPro-Light"'
ICON = '#8A8A8A'
PARKED = 'transform="matrix(1 0 0 1 640 320)"'

# Node map, shared with display.h.
BODY_NODE = 8
FIRST_INNER = 9                        # 9..11
CENTRE = 12
STRIKE = 13
FIRST_RIPPLE, RIPPLES = 14, 3          # 14..16
FIRST_STRING, STRINGS = 17, 4          # 17..20
FIRST_OUTLINE = 21                     # 21..32   the paisleys' outlines
FIRST_INNER_DROP = 33                  # 33..44   their inner teardrops
FIRST_EYE = 45                         # 45..56   their eyes
FIRST_PUPIL = 57                       # 57..68   their pupils
FIRST_DROP = 69                        # 69..80   the small drops
RING_INNER, RING_OUTER = 81, 82        # the wavy rings
FIRST_DOT_RING = 83                    # 83..94   the circled dots' rings
FIRST_DOT = 95                         # 95..106  and dots
FIRST_MODEL_LED = 107                  # 107..110
FIRST_MODES_LED = 111                  # 111..114
HALO = 115                             # 115 model, 116 modes
NODES = HALO + 2

# The motif lifted from the Rings panel (tools/extract_motif.py), scaled so the
# outermost dot reaches REACH pixels from the centre.
MOTIF = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'motif.json')))
MOTIF_REACH = max(math.hypot(x, y) for x, y in MOTIF['dot_ring'])
SCALE = REACH / MOTIF_REACH


def scaled(points):
    return [(x * SCALE, y * SCALE) for x, y in points]


def num(v):
    s = f'{v:.2f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def poly(points, stroke, width=1, fill='none', attrs=''):
    d = 'M' + ' L'.join(f'{num(x)},{num(y)}' for x, y in points) + ' Z'
    return f'<path{attrs} d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'


def node(i, points, stroke='#808080', width=1):
    return poly(points, stroke, width, attrs=f' id="{ID}_node_{i}" {PARKED}')


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
for i, colour in enumerate(BARS):
    out.append(f'<path id="{ID}_node_{i}" transform="matrix(1 0 0 1 {i * 60} 0)" d="M0,0 L60,0 L60,20 L0,20 Z" '
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

out.append('<!-- The LEDs\' icons, as on the Rings panel: models on the left, modes on the right. -->')
for k, y in enumerate(LED_Y):
    out += model_icon(k, LED_X[0], y)
    out += modes_icon(k, LED_X[1], y)

out.append('<!-- Everything below is placed by the display. The body: its circle, inner rings, strings and centre. -->')
out.append(circle_node(BODY_NODE, BODY, stroke='#A0A0A0'))
for i, r in enumerate(INNER):
    out.append(circle_node(FIRST_INNER + i, r))
out.append(f'<circle id="{ID}_node_{CENTRE}" {PARKED} cx="0" cy="0" r="2" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="0.5"/>')
out.append(f'<circle id="{ID}_node_{STRIKE}" {PARKED} cx="0" cy="0" r="2.5" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="1"/>')
for i in range(RIPPLES):
    out.append(circle_node(FIRST_RIPPLE + i, 40))
for i in range(STRINGS):
    out.append(node(FIRST_STRING + i, [(-STRING_HALF, -0.4), (STRING_HALF, -0.4), (STRING_HALF, 0.4), (-STRING_HALF, 0.4)], width=0.5))
out.append('<!-- The mandala, lifted from the Rings panel: twelve paisleys, two wavy rings and')
out.append('     twelve circled dots, every piece authored upright at the centre and turned by the display. -->')
P = MOTIF['paisley']
for first, key in ((FIRST_OUTLINE, 'outline'), (FIRST_INNER_DROP, 'inner'), (FIRST_EYE, 'eye'), (FIRST_PUPIL, 'pupil'), (FIRST_DROP, 'drop')):
    for i in range(PAISLEYS):
        out.append(node(first + i, scaled(P[key]), width=0.8))
out.append(node(RING_INNER, scaled(MOTIF['ring_inner']), width=0.8))
out.append(node(RING_OUTER, scaled(MOTIF['ring_outer']), width=0.8))
for i in range(PAISLEYS):
    out.append(node(FIRST_DOT_RING + i, scaled(MOTIF['dot_ring']), width=0.8))
for i in range(PAISLEYS):
    out.append(node(FIRST_DOT + i, scaled(MOTIF['dot']), width=0.8))
out.append('<!-- The LEDs and the halo that marks a change. -->')
for i in range(4):
    out.append(circle_node(FIRST_MODEL_LED + i, LED_R, stroke='#808080', width=1.2))
for i in range(4):
    out.append(circle_node(FIRST_MODES_LED + i, LED_R, stroke='#808080', width=1.2))
for i in range(2):
    out.append(circle_node(HALO + i, LED_R + 4, stroke='#808080', width=1))
for i in range(PALETTE):
    grey = f'#{i * 17:02x}{i * 17:02x}{i * 17:02x}'
    out.append(f'<circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{grey}" stroke="{grey}" stroke-width="1"/>')
out.append('</g>')
print('\n'.join(out))
