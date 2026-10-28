"""Reads Mutable's rings.svg (pdftocairo output of rings.ai) and returns the
Frequency mandala's subpaths flattened to polylines in screen pixels about (0, 0)."""
import math, re

CENTRE = (53.83, 94.62)      # the Frequency knob's centre, pt
RADIUS_PT = 36.0              # the mandala's reach, pt


def tokens(d):
    return re.findall(r'[A-Za-z]|-?\d*\.?\d+(?:e-?\d+)?', d)


def subpaths(d):
    """Absolute M/L/C/Z path data to a list of (closed, points) with curves flattened."""
    t = tokens(d)
    i = 0
    out, cur, closed = [], [], False
    x = y = 0.0
    while i < len(t):
        c = t[i]; i += 1
        if c == 'M':
            if cur: out.append((closed, cur))
            x, y = float(t[i]), float(t[i + 1]); i += 2
            cur, closed = [(x, y)], False
        elif c == 'L':
            x, y = float(t[i]), float(t[i + 1]); i += 2
            cur.append((x, y))
        elif c == 'C':
            x1, y1, x2, y2, x3, y3 = map(float, t[i:i + 6]); i += 6
            for k in range(1, 9):
                u = k / 8
                a, b, cc, dd = (1 - u) ** 3, 3 * u * (1 - u) ** 2, 3 * u * u * (1 - u), u ** 3
                cur.append((a * x + b * x1 + cc * x2 + dd * x3, a * y + b * y1 + cc * y2 + dd * y3))
            x, y = x3, y3
        elif c == 'Z':
            closed = True
            if cur:
                out.append((closed, cur)); cur = []
        else:
            raise ValueError(c)
    if cur: out.append((closed, cur))
    return out


def simplify(points, tolerance):
    """Douglas-Peucker on a closed polyline."""
    if len(points) < 4: return points
    def dp(pts):
        if len(pts) < 3: return pts
        (x0, y0), (x1, y1) = pts[0], pts[-1]
        dx, dy = x1 - x0, y1 - y0
        n = math.hypot(dx, dy) or 1e-9
        best, index = 0, 0
        for k in range(1, len(pts) - 1):
            px, py = pts[k]
            dist = abs(dy * (px - x0) - dx * (py - y0)) / n
            if dist > best: best, index = dist, k
        if best > tolerance:
            return dp(pts[:index + 1])[:-1] + dp(pts[index:])
        return [pts[0], pts[-1]]
    # split at the farthest point from the first, so the closed loop has two open halves
    far = max(range(len(points)), key=lambda k: math.hypot(points[k][0] - points[0][0], points[k][1] - points[0][1]))
    a = dp(points[:far + 1])
    b = dp(points[far:] + [points[0]])
    result = a[:-1] + b[:-1]
    # drop near-duplicates
    cleaned = []
    for p in result:
        if not cleaned or math.hypot(p[0] - cleaned[-1][0], p[1] - cleaned[-1][1]) > 0.05:
            cleaned.append(p)
    if len(cleaned) > 1 and math.hypot(cleaned[0][0] - cleaned[-1][0], cleaned[0][1] - cleaned[-1][1]) <= 0.05:
        cleaned.pop()
    return cleaned


def mandala(svg_text, scale, tolerance=0.35):
    """Every subpath within the mandala's reach, in screen pixels about the centre,
    simplified. Returns a list of dicts: points, r, angle (degrees, clockwise from up), fill."""
    found = []
    for m in re.finditer(r'<path([^>]*) d="([^"]*)"', svg_text):
        attrs, d = m.group(1), m.group(2)
        fill = re.search(r'fill="([^"]*)"', attrs)
        fill = fill.group(1) if fill else ''
        if 'rgb(9' in fill or 'rgb(8' in fill.replace('87', 'x') and False:
            pass
        for closed, pts in subpaths(d):
            xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
            cx, cy = sum(xs) / len(xs), sum(ys) / len(ys)
            dist = math.hypot(cx - CENTRE[0], cy - CENTRE[1])
            if dist > RADIUS_PT: continue
            if max(xs) - min(xs) > 50 or max(ys) - min(ys) > 50: continue  # the panel outline
            local = [((x - CENTRE[0]) * scale, (y - CENTRE[1]) * scale) for x, y in pts]
            local = simplify(local, tolerance)
            if len(local) < 3: continue
            lcx, lcy = sum(p[0] for p in local) / len(local), sum(p[1] for p in local) / len(local)
            found.append({'points': local, 'r': math.hypot(lcx, lcy), 'angle': (math.degrees(math.atan2(lcx, -lcy)) + 360) % 360,
                          'fill': fill, 'closed': closed})
    return found


if __name__ == '__main__':
    import sys
    svg = open(sys.argv[1]).read()
    parts = mandala(svg, 88 / RADIUS_PT * 36 / 33)
    print(len(parts), 'subpaths;', sum(len(p['points']) for p in parts), 'points')
    fills = {}
    for p in parts: fills[p['fill']] = fills.get(p['fill'], 0) + 1
    print(fills)
    by_r = sorted(parts, key=lambda p: p['r'])
    for p in by_r[:: max(1, len(by_r) // 40)]:
        print(f"r {p['r']:6.1f} angle {p['angle']:6.1f} points {len(p['points']):3d} fill {p['fill'][:22]}")
