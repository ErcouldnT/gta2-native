#!/usr/bin/env python3
"""Checks translation batches against the source: '#' highlight count, %d/%s placeholders,
the leading speaker glyph and '*' separators must match."""
import glob, json, os, re, sys
d = sys.argv[1]
uniq = json.load(open(os.path.join(d, 'uniq.json'), encoding='utf-8'))
bad = 0
for f in sorted(glob.glob(os.path.join(d, 'out_*.tsv'))):
    for line in open(f, encoding='utf-8'):
        line = line.rstrip('\n')
        if not line.strip():
            continue
        idx, t = line.split('\t', 1)
        s = uniq[int(idx)]
        probs = []
        if s.count('#') != t.count('#'):
            probs.append(f"# {s.count('#')}!={t.count('#')}")
        if re.findall(r'%[ds]', s) != re.findall(r'%[ds]', t):
            probs.append('placeholders')
        if s[:1] != t[:1] and ord(s[0]) > 0x2000:
            probs.append('speaker')
        if s.count('*') != t.count('*'):
            probs.append('*')
        if probs:
            bad += 1
            print(f'{os.path.basename(f)} {idx}: {", ".join(probs)}\n  EN: {s}\n  TR: {t}')
print('problems:', bad)
