#!/usr/bin/env python3
"""Generates panel.svg.fragment: the two-page header block, the paisley filigree
either side, the ring tile, and the nodes the display animates.

Only forms seen on hardware: rect, circle, path of M/L/Z, text; at most two
decimals; explicit paint on every leaf; bound nodes parked off screen.
"""
import math

ID = 'mi_rings'
CX, CY = 240, 130       # the tile's centre
HALF = 86               # the tile's half side
FIRST_RING, RINGS = 8, 12
STRIKE = FIRST_RING + RINGS
FIRST_RIPPLE, RIPPLES = STRIKE + 1, 3
PALETTE = 16
BARS = ['#000000', '#323232', '#4B4B4B', '#646464', '#7D7D7D', '#969696', '#AFAFAF', '#CDCDCD']
FONT = 'font-family="UniversNextPro-Light"'


def num(v):
    s = f'{v:.2f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def poly(points, stroke, width=1, fill='none', close=True, attrs=''):
    d = 'M' + ' L'.join(f'{num(x)},{num(y)}' for x, y in points) + (' Z' if close else '')
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"{attrs}/>'


def paisley(length, width, bend, steps):
    """A paisley outline: a slender teardrop bent along an arc, one closed
    polyline in local coordinates. It starts at the origin heading +x and curls
    towards +y; the tip is at the far end. Every point is distinct."""
    radius = length / math.radians(bend)
    def centre(u):
        phi = u * math.radians(bend)
        return (radius * math.sin(phi), radius * (1 - math.cos(phi))), (-math.sin(phi), math.cos(phi))
    def half_width(u):
        return width * 2.2 * math.sqrt(u) * (1 - u) ** 1.3
    left, right = [], []
    for i in range(1, steps):
        u = i / steps
        (px, py), (nx, ny) = centre(u)
        w = half_width(u)
        left.append((px + nx * w, py + ny * w))
        right.append((px - nx * w, py - ny * w))
    base, tip = centre(0)[0], centre(1)[0]
    return [base] + left + [tip] + right[::-1]


def ornament(ox, oy, length, turn, mirror, dots=True):
    """One paisley with its base at (ox, oy), turned by `turn` degrees clockwise;
    mirrored ones curl the other way."""
    m = -1 if mirror else 1
    a = math.radians(turn * m)
    c, s_ = math.cos(a), math.sin(a)
    def put(pts):
        return [(ox + (m * x) * c - y * s_, oy + (m * x) * s_ + y * c) for x, y in pts]
    width = length * 0.3
    bend = 120
    outer = paisley(length, width, bend, 26)
    # The inner paisley and the lace dots are the outline shrunk about its centroid.
    cx = sum(x for x, _ in outer) / len(outer)
    cy = sum(y for _, y in outer) / len(outer)
    def shrink(pts, k):
        return [(cx + (x - cx) * k, cy + (y - cy) * k) for x, y in pts]
    inner = shrink(paisley(length, width, bend, 18), 0.5)
    lines = [poly(put(outer), '#333333'), poly(put(inner), '#262626')]
    if dots:
        for x, y in put(shrink(paisley(length, width, bend, 12), 0.76))[2:9]:
            lines.append(f'<circle cx="{num(x)}" cy="{num(y)}" r="1" fill="#333333" stroke="#333333" stroke-width="0.5"/>')
    return lines


out = [f'<g id="{ID}_panel">',
       '<!-- Two-page header in the proven hurdygurdy form: a black rect covers the stock header,',
       '     then the bar steps and both pages\' titles and values at their resting sizes. -->',
       '<rect x="0" y="0" width="480" height="220" fill="#000000"/>']
for i, colour in enumerate(BARS):
    out.append(f'<path id="{ID}_node_{i}" transform="matrix(1 0 0 1 {i * 60} 0)" d="M0,0 L60,0 L60,20 L0,20 Z" '
               f'fill="{colour}" stroke="{colour}" stroke-width="0.5"/>')
titles = ['struct', 'bright', 'damping', 'position', 'modes', 'strike', 'damping', 'position']
values = ['50', '50', '50', '50', '4', '50', '50', '50']
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

out.append('<!-- Paisley filigree either side of the tile, after the Mutable Instruments panels. -->')
for mirror, sx in ((False, 1), (True, -1)):
    # Either side of the tile: a large paisley rising from the bottom and curling
    # outwards, a small one above it curling back, and a bud near the tile.
    side = CX - sx * (HALF + 76)          # the side area's centre, 76 px off the tile
    out += ornament(side - sx * 6, 142, 80, 22, mirror)
    out += ornament(side - sx * 34, 78, 42, 30, not mirror, dots=False)
    out += ornament(side + sx * 40, 92, 34, 200, mirror, dots=False)

out.append('<!-- The tile: a thin square, as the Rings logo frames its ripples. -->')
out.append(poly([(CX - HALF, CY - HALF), (CX + HALF, CY - HALF), (CX + HALF, CY + HALF), (CX - HALF, CY + HALF)], '#3A3A3A'))
out.append('<!-- Rings, one per partial, the strike point, and ripples from it: all placed by the display. -->')
for i in range(RINGS):
    out.append(f'<circle id="{ID}_node_{FIRST_RING + i}" transform="matrix(1 0 0 1 640 320)" cx="0" cy="0" r="40" fill="none" stroke="#808080" stroke-width="1"/>')
out.append(f'<circle id="{ID}_node_{STRIKE}" transform="matrix(1 0 0 1 640 320)" cx="0" cy="0" r="2.5" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="1"/>')
for i in range(RIPPLES):
    out.append(f'<circle id="{ID}_node_{FIRST_RIPPLE + i}" transform="matrix(1 0 0 1 640 320)" cx="0" cy="0" r="40" fill="none" stroke="#808080" stroke-width="1"/>')
for i in range(PALETTE):
    grey = f'#{i * 17:02x}{i * 17:02x}{i * 17:02x}'
    out.append(f'<circle id="{ID}_palette_{i}" cx="640" cy="0" r="1" fill="{grey}" stroke="{grey}" stroke-width="1"/>')
out.append('</g>')
print('\n'.join(out))
