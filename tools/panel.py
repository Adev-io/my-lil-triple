#!/usr/bin/env python3
"""panel.py: writes res/Triple.svg and src/layout.hpp from one layout table.

Labels become paths (DejaVu Sans from the Rack install, or the font named by RACK_FONT), so the SVG has no <text>.
The widgets read the same table through layout.hpp, so art and components cannot drift.
House style as in My lil Drums / Looper / Acid Bass. Run from anywhere: py -3.11 tools/panel.py   (needs fontTools).
Author: Ari Klopfer / A_Development. MIT
"""
import math
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

H = 128.5
W = 5.08 * 40
BAND = 8.6
INK, INK2 = '#16161a', '#202027'
LIME, MAGENTA, BLUE, WHITE, DIM = '#c8ff3a', '#ff3d9a', '#5ac8e6', '#f2f2f4', '#8d8d96'
FONT = os.environ.get('RACK_FONT', r'C:\Program Files\VCV\Rack2Free\res\fonts\DejaVuSans.ttf')
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

RADIUS = {'knob': 6.7, 'button': 3.1, 'seq': 4.0, 'play': 4.0, 'cv': 4.0, 'out': 4.0}
LABEL_SIZE = {'knob': 2.4, 'button': 2.0, 'seq': 2.4, 'play': 2.4, 'cv': 2.4, 'out': 2.2}
RING_COLOR = {'seq': BLUE, 'play': LIME, 'cv': MAGENTA}
PAD_R = 4.8
RING_R = RADIUS['cv'] + 0.7

font = TTFont(FONT)
glyphs, cmap, upm = font.getGlyphSet(), font.getBestCmap(), font['head'].unitsPerEm


def width(s, size):
    return sum(glyphs[cmap[ord(c)]].width for c in s) * size / upm


def text(s, x, y, size, fill, anchor='middle', stroke=0.0):
    """One <path> for the string: baseline at y, em size in mm, optional stroke for a bolder face."""
    k = size / upm
    w = width(s, size)
    pen_x = x - w / 2 if anchor == 'middle' else x - w if anchor == 'end' else x
    d = []
    for c in s:
        g = glyphs[cmap[ord(c)]]
        pen = SVGPathPen(glyphs)
        g.draw(TransformPen(pen, (k, 0, 0, -k, pen_x, y)))
        d.append(pen.getCommands())
        pen_x += g.width * k
    bold = ' stroke="%s" stroke-width="%g" stroke-linejoin="round"' % (fill, stroke) if stroke else ''
    return '<path fill="%s"%s d="%s"/>' % (fill, bold, ' '.join(d))


TITLE, TITLE_X, TITLE_Y, TITLE_SIZE = 'MY LIL TRIPLE', 11.2, 6.3, 4.3
MASCOT_X = W - 14.46
MASCOT_R = BAND * 0.36


def mascot(cx, cy, r):
    e = r * 0.37
    return ''.join([
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx, cy, r, MAGENTA),
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx - e, cy - r * 0.32, r * 0.145, INK),
        '<circle cx="%g" cy="%g" r="%g" fill="%s"/>' % (cx + e, cy - r * 0.32, r * 0.145, INK),
        '<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="%g" stroke-linecap="round"/>'
        % (cx - r * 0.58, cy + r * 0.45, cx + r * 0.58, cy + r * 0.45, INK, r * 0.16),
    ])


# ---- components: (name, kind, x, y, label). Twelve columns, three rows, jacks below.
CX = [14.0 + 16.0 * i for i in range(12)]
ROW_A, ROW_B, ROW_C, JACK_Y = 26.0, 50.0, 74.0, 108.0
OSC_ROWS = [ROW_A, ROW_B, ROW_C]
OSC_PARTS = [('WAVE', 'knob'), ('INV', 'button'), ('PHASE', 'knob'), ('COARSE', 'knob'), ('FINE', 'knob'), ('PAN', 'knob'), ('MIX', 'knob')]

LAYOUT = []
for o in range(3):
    for j, (label, kind) in enumerate(OSC_PARTS):
        if o == 0 and label == 'MIX':
            continue   # osc 1 has no mix: it is the reference level
        LAYOUT.append(('OSC%d_%d' % (o, j), kind, CX[j], OSC_ROWS[o], label))
LAYOUT += [
    ('AM_PARAM', 'button', CX[6], ROW_A, 'AM OSC3'), ('HQ_PARAM', 'button', CX[7], ROW_A, 'HQ'),
    ('A_ATK_PARAM', 'knob', CX[8], ROW_A, 'ATK'), ('A_DEC_PARAM', 'knob', CX[9], ROW_A, 'DEC'),
    ('A_SUS_PARAM', 'knob', CX[10], ROW_A, 'SUS'), ('A_REL_PARAM', 'knob', CX[11], ROW_A, 'REL'),
    ('FTYPE_PARAM', 'knob', CX[7], ROW_B, 'TYPE'), ('CUTOFF_PARAM', 'knob', CX[8], ROW_B, 'CUTOFF'),
    ('RES_PARAM', 'knob', CX[9], ROW_B, 'RES'), ('ENVAMT_PARAM', 'knob', CX[10], ROW_B, 'ENV'),
    ('KEYTRK_PARAM', 'knob', CX[11], ROW_B, 'KEY'),
    ('F_ATK_PARAM', 'knob', CX[8], ROW_C, 'ATK'), ('F_DEC_PARAM', 'knob', CX[9], ROW_C, 'DEC'),
    ('F_SUS_PARAM', 'knob', CX[10], ROW_C, 'SUS'), ('F_REL_PARAM', 'knob', CX[11], ROW_C, 'REL'),
    ('VOCT_INPUT', 'play', CX[0], JACK_Y, 'V/OCT'), ('GATE_INPUT', 'play', CX[1], JACK_Y, 'GATE'),
    ('CUTCV_INPUT', 'cv', CX[2], JACK_Y, 'CUTOFF'),
    ('OUT_L_OUTPUT', 'out', CX[10], JACK_Y, 'OUT L'), ('OUT_R_OUTPUT', 'out', CX[11], JACK_Y, 'OUT R'),
]
CAPTIONS = [('OSC 1', CX[0], ROW_A), ('OSC 2', CX[0], ROW_B), ('OSC 3', CX[0], ROW_C),
            ('AMP ENV', CX[8], ROW_A), ('FILTER', CX[8], ROW_B), ('FILTER ENV', CX[8], ROW_C)]


def reach(kind):
    return RING_R if kind in RING_COLOR else PAD_R if kind == 'out' else RADIUS[kind]


def label_base(kind, y):
    return y + reach(kind) + (2.6 if kind != 'knob' else 3.2)


def problems():
    """Parts must not overlap each other, their labels, the band, the screws or the panel edge. Must be empty."""
    bad = []
    parts = [(n, x, y, reach(k)) for n, k, x, y, _ in LAYOUT]
    for j, (a, ax, ay, ar) in enumerate(parts):
        if ax - ar < 0 or ax + ar > W or ay - ar < BAND or ay + ar > H:
            bad.append('%s leaves the panel or enters the band' % a)
        for b, bx, by, br in parts[j + 1:]:
            if math.hypot(ax - bx, ay - by) < ar + br:
                bad.append('%s touches %s' % (a, b))
        for sx in (5.08, W - 10.16):
            for sy in (0.0, H - 5.08):
                if sx < ax + ar and ax - ar < sx + 5.08 and sy < ay + ar and ay - ar < sy + 5.08:
                    bad.append('%s under a screw' % a)
    for n, k, x, y, lab in LAYOUT:   # a label must clear every other part
        w, base, size = width(lab, LABEL_SIZE[k]), label_base(k, y), LABEL_SIZE[k]
        box = (x - w / 2, base - 0.73 * size, x + w / 2, base)
        for m, mx, my, mr in parts:
            if m != n:
                nx, ny = min(max(mx, box[0]), box[2]), min(max(my, box[1]), box[3])
                if math.hypot(mx - nx, my - ny) < mr:
                    bad.append('%s label covers %s' % (n, m))
        if box[2] > W or box[3] > H:
            bad.append('%s label leaves the panel' % n)
    return bad


def panel():
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="%gmm" height="%gmm" viewBox="0 0 %g %g">' % (W, H, W, H),
        '<rect width="%g" height="%g" fill="%s"/>' % (W, H, INK),
        '<rect width="%g" height="%g" fill="%s"/>' % (W, BAND, LIME),
        text(TITLE, TITLE_X, TITLE_Y, TITLE_SIZE, INK, 'start', 0.39), mascot(MASCOT_X, BAND / 2, MASCOT_R),
        '<rect x="2.5" y="94" width="%g" height="32" rx="2" fill="%s"/>' % (W - 5, INK2),
    ]
    for cap, x, y in CAPTIONS:
        out.append(text(cap, x - RADIUS['knob'], y - RADIUS['knob'] - 1.7, 2.2, DIM, 'start'))
    for _, kind, x, y, label in LAYOUT:
        if kind in RING_COLOR:
            out.append('<circle cx="%g" cy="%g" r="%g" fill="none" stroke="%s" stroke-width="0.5"/>' % (x, y, RING_R, RING_COLOR[kind]))
        elif kind == 'out':
            out.append('<circle cx="%g" cy="%g" r="%g" fill="%s" stroke="%s" stroke-width="0.5"/>' % (x, y, PAD_R, INK, LIME))
        colour, stroke = (LIME, 0.2) if kind == 'out' else (WHITE, 0.15)
        out.append(text(label, x, label_base(kind, y), LABEL_SIZE[kind], colour, stroke=stroke))
    out.append(text('A_DEVELOPMENT', W - 6, 127.2, 2.2, DIM, 'end'))
    out.append('</svg>')
    return '\n'.join(out) + '\n'


def header():
    pos = {n: (x, y) for n, k, x, y, _ in LAYOUT}
    osc = []
    for o in range(3):
        row = [pos.get('OSC%d_%d' % (o, j), (0.0, 0.0)) for j in range(len(OSC_PARTS))]   # osc 1's MIX stays at 0,0 and is skipped
        osc.append('{%s}' % ', '.join('{%.2ff, %.2ff}' % p for p in row))
    lines = ['// layout.hpp: generated by tools/panel.py, do not edit. Component centres in mm.', '#pragma once',
             'namespace layout {', 'namespace triple {',
             'constexpr float OSC[3][%d][2] = {%s};   // wave, invert, phase, coarse, fine, pan, mix' % (len(OSC_PARTS), ', '.join(osc))]
    lines += ['constexpr float %s[2] = {%.2ff, %.2ff};' % (n, x, y) for n, k, x, y, _ in LAYOUT if not n.startswith('OSC')]
    lines += ['constexpr float FACE[3] = {%.2ff, %.2ff, %.3ff};   // mascot centre x, y, radius (TalkingFace)' % (MASCOT_X, BAND / 2, MASCOT_R)]
    lines += ['}', '}', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    bad = problems()
    assert not bad, '; '.join(bad)
    files = {os.path.join('res', 'Triple.svg'): panel(), os.path.join('src', 'layout.hpp'): header()}
    for rel, body in files.items():
        os.makedirs(os.path.dirname(os.path.join(ROOT, rel)), exist_ok=True)
        with open(os.path.join(ROOT, rel), 'w', encoding='utf-8') as f:
            f.write(body)
    print('wrote', ', '.join(files), '(%d components)' % len(LAYOUT))
