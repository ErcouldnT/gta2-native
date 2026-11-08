#!/usr/bin/env python3
"""Builds the Turkish e.gxt from the English dump and the translation batches.
  build_tr.py <tr dir> <out.gxt>
tr dir: e.json (gxt.py dump of the English e.gxt), uniq.json (unique source strings),
out_*.tsv (index<TAB>Turkish). Untranslated strings stay English.
Characters are written with GTA2's font codes (character c draws glyph c - 33):
 --font=native   (default) needs style files patched by sty_tr_font.py: Ç Ö Ü ç ö ü â use the
                 fonts' own Western European glyphs, ğ Ğ ş Ş ı İ the added ones
 --font=ascii    unpatched fonts: Turkish letters become their closest ASCII letters
 --keep-turkish  raw Unicode (for inspection only, renders as '!')"""
import glob, json, os, subprocess, sys

# glyph offset + 33; see sty_tr_font.py for the layout
NATIVE = str.maketrans({'Ç': 133, 'Ö': 145, 'Ü': 149, 'ç': 156, 'ö': 168, 'ü': 172, 'Â': 130, 'â': 153,
                        'î': 163, 'û': 171,
                        'ğ': 178, 'Ğ': 179, 'ş': 180, 'Ş': 181, 'ı': 182, 'İ': 183})
TRANSLIT = str.maketrans({'ç': 'c', 'Ç': 'C', 'ğ': 'g', 'Ğ': 'G', 'ı': 'i', 'İ': 'I', 'ö': 'o', 'Ö': 'O',
                          'ş': 's', 'Ş': 'S', 'ü': 'u', 'Ü': 'U', 'â': 'a', 'Â': 'A', 'î': 'i', 'û': 'u'})

def main():
    d, out = [a for a in sys.argv[1:] if not a.startswith('--')][:2]
    mode = 'raw' if '--keep-turkish' in sys.argv else 'ascii' if '--font=ascii' in sys.argv else 'native'
    table = {'raw': {}, 'ascii': TRANSLIT, 'native': NATIVE}[mode]
    j = json.load(open(os.path.join(d, 'e.json'), encoding='utf-8'))
    uniq = json.load(open(os.path.join(d, 'uniq.json'), encoding='utf-8'))
    tr = {}
    for f in sorted(glob.glob(os.path.join(d, 'out_*.tsv'))):
        for line in open(f, encoding='utf-8'):
            line = line.rstrip('\n')
            if not line.strip():
                continue
            idx, text = line.split('\t', 1)
            tr[uniq[int(idx)]] = text
    done = 0
    for k, v in j['strings'].items():
        if v in tr:
            t = tr[v]
            j['strings'][k] = t.translate(table)
            done += 1
    tmp = os.path.join(d, '_build.json')
    json.dump(j, open(tmp, 'w', encoding='utf-8'), ensure_ascii=False)
    here = os.path.dirname(os.path.abspath(__file__))
    subprocess.check_call([sys.executable, '-I', os.path.join(here, 'gxt.py'), 'build', tmp, out])
    print(f'translated {done}/{len(j["strings"])} strings ({len(tr)}/{len(uniq)} unique)')

main()
