#!/usr/bin/env python3
"""Adds the Turkish letters GTA2's fonts lack to a style file (.sty).

  sty_tr_font.py <in.sty> <out.sty>

GTA2 draws character c of font f with sprite (font base + c - 33). The 140/144-glyph fonts
already hold the Western European letters after 'z' (offset 95 = À ... 139 = ü), which covers
Ç Ö Ü ç ö ü â. The six missing letters are synthesised from existing glyphs and appended
after offset 144 (fonts are padded to 145 glyphs first; some in-game fonts already have 145):

  offset 145 ğ   146 Ğ   147 ş   148 Ş   149 ı   150 İ     (character codes 178..183)

build_tr.py writes GXT text with these codes. Fonts are the last sprite type in a style
file and their palettes the last sprite palettes, so appending font sprites only shifts
the later (remap) virtual palettes, which are addressed through PALB."""
import struct, sys

TR_FIRST = 145  # offset of the first added glyph; order ğ Ğ ş Ş ı İ

def off(ch):
    return ord(ch) - 33

# Existing glyph offsets of the Western European block
LATIN = {'À': 95, 'Â': 97, 'Ä': 98, 'Ç': 100, 'Ö': 112, 'Ü': 116, 'â': 120, 'ä': 121, 'ç': 123, 'ö': 135, 'ü': 139}


def read_chunks(d):
    assert d[:4] == b'GBST', 'not a GTA2 style file'
    chunks, p = [], 6
    while p < len(d):
        t = d[p:p + 4]
        n = struct.unpack('<I', d[p + 4:p + 8])[0]
        chunks.append([t, d[p + 8:p + 8 + n]])
        p += 8 + n
    return chunks


class Glyph:
    def __init__(self, w, h, px, pal):
        self.w, self.h, self.px, self.pal = w, h, px, pal  # px: list of rows (lists of palette indices)

    def copy(self):
        return Glyph(self.w, self.h, [r[:] for r in self.px], self.pal)

    def blank(self):
        return sum(1 for r in self.px for v in r if v) < 4

    def bbox(self):
        xs = [x for r in self.px for x, v in enumerate(r) if v]
        ys = [y for y, r in enumerate(self.px) if any(r)]
        return (min(xs), min(ys), max(xs), max(ys)) if xs else (0, 0, 0, 0)


def diff(accented, base):
    """Pixels of `accented` that are not in `base` (the accent), as (x, y, v)."""
    out = []
    for y in range(min(accented.h, base.h)):
        for x in range(accented.w):
            v = accented.px[y][x]
            b = base.px[y][x] if x < base.w else 0
            if v and v != b:
                out.append((x, y, v))
    return out


def stamp(target, pixels, src_base, flip=False):
    """Draws accent pixels onto target, centred like they were on src_base."""
    if not pixels:
        return target
    sb, tb = src_base.bbox(), target.bbox()
    dx = round((tb[0] + tb[2]) / 2 - (sb[0] + sb[2]) / 2)
    ys = [y for _, y, _ in pixels]
    y0, y1 = min(ys), max(ys)
    g = target.copy()
    for x, y, v in pixels:
        if flip:
            y = y0 + y1 - y
        nx = x + dx
        if 0 <= nx < g.w and 0 <= y < g.h:
            g.px[y][nx] = v
    return g


def accent_on(target, accented, base, flip=False, keep_left=False, breve=False):
    """Puts the accent of `accented` (= `base` + accent above it) onto `target`.
    Some fonts shrink the letter under an accent; then `target` is shrunk the same way."""
    ab, cb = base.bbox(), accented.bbox()
    same = all(accented.px[y][:base.w] == base.px[y][:accented.w] for y in range(ab[1] + 1, ab[3] + 1)) \
        if accented.w == base.w else False
    if same:  # letter unchanged, accent is whatever was added on top
        acc = [(x, y, v) for x, y, v in diff(accented, base) if y <= ab[1]]
        g, letter_top = target.copy(), None
    else:  # accent, a gap, then the (shrunk) letter
        y = cb[1]
        while y < accented.h and any(accented.px[y]):
            y += 1
        while y < accented.h and not any(accented.px[y]):
            y += 1
        letter_top = y if y <= ab[3] else ab[1]
        acc = [(x, yy, v) for yy in range(cb[1], letter_top) for x, v in enumerate(accented.px[yy]) if v]
        tb = target.bbox()
        g = target.copy()
        g.px = [[0] * g.w for _ in range(g.h)]
        new_h = ab[3] - letter_top + 1
        src_h = tb[3] - tb[1] + 1
        for r in range(new_h):
            g.px[letter_top + r] = target.px[tb[1] + r * src_h // new_h][:]
    if keep_left and acc:
        mid = (min(x for x, _, _ in acc) + max(x for x, _, _ in acc)) / 2
        acc = [p for p in acc if p[0] < mid]
    if breve and acc:
        acc = dots_to_breve(acc)
    if not acc:
        return g
    ys = [y for _, y, _ in acc]
    y0, y1 = min(ys), max(ys)
    xs = [x for x, _, _ in acc]
    tb = g.bbox()
    letter_cx = (ab[0] + ab[2]) / 2 if same else (min(x for y in range(letter_top or 0, accented.h) for x, v in enumerate(accented.px[y]) if v) + max(x for y in range(letter_top or 0, accented.h) for x, v in enumerate(accented.px[y]) if v)) / 2
    if keep_left:
        letter_cx = (min(xs) + max(xs)) / 2
    dx = round((tb[0] + tb[2]) / 2 - letter_cx)
    for x, y, v in acc:
        if flip:
            y = y0 + y1 - y
        nx = x + dx
        if 0 <= nx < g.w and 0 <= y < g.h:
            g.px[y][nx] = v
    return g


def dots_to_breve(acc):
    """Draws a breve (a U shape) in the box and colours of an umlaut accent: the outline
    colour on its edge, inside each row's most common inner colour (fonts shade by row)."""
    from collections import Counter
    pts = {(x, y): v for x, y, v in acc}
    x0, x1 = min(x for x, _ in pts), max(x for x, _ in pts)
    y0, y1 = min(y for _, y in pts), max(y for _, y in pts)
    w, h = x1 - x0 + 1, y1 - y0 + 1
    edge = [v for (x, y), v in pts.items() if x in (x0, x1) or y in (y0, y1)]
    outline = Counter(edge).most_common(1)[0][0]
    side, bottom = max(2, round(w * 0.3)), max(2, round(h * 0.45))

    def inside(x, y):
        return x0 <= x <= x1 and y0 <= y <= y1 and (x < x0 + side or x > x1 - side or y > y1 - bottom)

    row_colour = {}
    for y in range(y0, y1 + 1):
        inner = [v for (x, yy), v in pts.items() if yy == y and v != outline]
        row_colour[y] = Counter(inner).most_common(1)[0][0] if inner else outline
    out = []
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if not inside(x, y):
                continue
            border = h > 3 and w > 4 and not all(inside(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            out.append((x, y, outline if border else row_colour[y]))
    return out


def cedilla(accented, base):
    """The cedilla of `accented`: added pixels from the base letter's bottom row down."""
    bottom = base.bbox()[3]
    return [(x, y, v) for x, y, v in diff(accented, base) if y >= bottom]


def make_turkish(glyph):
    """glyph(offset) -> Glyph. Returns the six new letters in order ğ Ğ ş Ş ı İ."""
    up_only = glyph(off('a')).blank()
    A, C = glyph(off('A')), glyph(off('C'))
    G_up = accent_on(glyph(off('G')), glyph(LATIN['Ä']), A, breve=True)
    S_up = stamp(glyph(off('S')), cedilla(glyph(LATIN['Ç']), C), C)
    I_dot = accent_on(glyph(off('I')), glyph(LATIN['Ä']), A, keep_left=True)
    if up_only:
        return [G_up, G_up, S_up, S_up, glyph(off('I')).copy(), I_dot]
    a, c = glyph(off('a')), glyph(off('c'))
    g_lo = accent_on(glyph(off('g')), glyph(LATIN['ä']), a, breve=True)
    s_lo = stamp(glyph(off('s')), cedilla(glyph(LATIN['ç']), c), c)
    i = glyph(off('i')).copy()
    for y in range(c.bbox()[1]):  # 'i' minus everything above the x-height (its dot)
        i.px[y] = [0] * i.w
    # İ: the dot of 'i' moved up onto 'I'
    src = glyph(off('i'))
    pix = [(x, y, v) for y in range(c.bbox()[1]) for x, v in enumerate(src.px[y]) if v]
    shift = glyph(off('I')).bbox()[1] - max(y for _, y, _ in pix)
    if min(y for _, y, _ in pix) + shift >= 0:
        I_dot = stamp(glyph(off('I')), [(x, y + shift, v) for x, y, v in pix], src)
    return [g_lo, G_up, s_lo, S_up, i, I_dot]


def patch(data):
    chunks = read_chunks(data)
    get = {t: c for t, c in chunks}
    sprb = list(struct.unpack('<6H', get[b'SPRB'][:12]))
    palb = list(struct.unpack('<8H', get[b'PALB'][:16]))
    fonb = get[b'FONB']
    nfonts = struct.unpack('<H', fonb[:2])[0]
    counts = list(struct.unpack('<%dH' % nfonts, fonb[2:2 + 2 * nfonts]))
    sprx, sprg = get[b'SPRX'], bytearray(get[b'SPRG'])
    palx = list(struct.unpack('<%dH' % (len(get[b'PALX']) // 2), get[b'PALX']))
    nspr = len(sprx) // 8
    font_start = sum(sprb[:5])
    assert font_start + sum(counts) == nspr == palb[1], 'unexpected sprite layout'
    if any(c > 145 for c in counts):
        raise SystemExit('fonts already patched')

    entries = [list(struct.unpack('<IBBH', sprx[i * 8:i * 8 + 8])) for i in range(nspr)]
    spr_pal = [palx[palb[0] + i] for i in range(nspr)]  # physical palette of each sprite

    def load(i):
        ptr, w, h, _ = entries[i]
        return Glyph(w, h, [list(sprg[ptr + y * 256:ptr + y * 256 + w]) for y in range(h)], spr_pal[i])

    # New sprite graphics go on fresh 256x256 pages, packed in shelves
    page_base = len(sprg)
    new_pages = bytearray()
    cur = [0, 0, 0]  # x, y, shelf height

    def place(g):
        # cur[1] is a row in the whole new-page area (page n spans rows 256n..256n+255)
        if cur[0] + g.w > 256:
            cur[0], cur[1], cur[2] = 0, cur[1] + cur[2], 0
        if new_pages and cur[1] // 256 != (cur[1] + g.h - 1) // 256:
            cur[0], cur[1], cur[2] = 0, (cur[1] // 256 + 1) * 256, 0  # don't straddle pages
        while cur[1] + g.h > len(new_pages) // 256:
            new_pages.extend(bytes(65536))
        x, y = cur[0], cur[1]
        for r, row in enumerate(g.px):
            o = (y + r) * 256 + x
            new_pages[o:o + g.w] = bytes(row)
        cur[0] += g.w
        cur[2] = max(cur[2], g.h)
        return page_base + y * 256 + x

    out_entries = entries[:font_start]
    out_pal = spr_pal[:font_start]
    new_counts = []
    s = font_start
    for f, c in enumerate(counts):
        idx = list(range(s, s + c))
        s += c
        fe = [entries[i][:] for i in idx]
        fp = [spr_pal[i] for i in idx]
        if c >= 140:
            glyph = lambda k, idx=idx: load(idx[k])
            while len(fe) < TR_FIRST:  # pad to a common length
                fe.append(entries[idx[0]][:])
                fp.append(spr_pal[idx[0]])
            tr = make_turkish(glyph)
            for g in tr:
                fe.append([place(g), g.w, g.h, 0])
                fp.append(g.pal)
        out_entries += fe
        out_pal += fp
        new_counts.append(len(fe))

    added = len(out_entries) - nspr
    sprb[5] += added
    new_palx = palx[:palb[0]] + out_pal + palx[palb[0] + nspr:sum(palb)]
    assert len(new_palx) <= len(palx), 'out of virtual palettes'
    new_palx += [0] * (len(palx) - len(new_palx))
    palb[1] += added

    rebuilt = {
        b'SPRB': struct.pack('<6H', *sprb) + get[b'SPRB'][12:],
        b'PALB': struct.pack('<8H', *palb) + get[b'PALB'][16:],
        b'FONB': struct.pack('<H', nfonts) + struct.pack('<%dH' % nfonts, *new_counts) + fonb[2 + 2 * nfonts:],
        b'SPRX': b''.join(struct.pack('<IBBH', *e) for e in out_entries),
        b'SPRG': bytes(sprg) + bytes(new_pages),
        b'PALX': struct.pack('<%dH' % len(new_palx), *new_palx),
    }
    out = bytearray(data[:6])
    for t, c in chunks:
        c = rebuilt.get(t, c)
        out += t + struct.pack('<I', len(c)) + c
    return bytes(out), counts, new_counts


if __name__ == '__main__':
    src, dst = sys.argv[1], sys.argv[2]
    data, before, after = patch(open(src, 'rb').read())
    open(dst, 'wb').write(data)
    print(f'{src}: font glyphs {before} -> {after}')
