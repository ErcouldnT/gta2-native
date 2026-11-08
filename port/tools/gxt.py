#!/usr/bin/env python3
"""GTA2 .gxt text file tool.
  gxt.py dump  file.gxt out.json      -> {"header": hex, "order": [keys], "strings": {key: text}}
  gxt.py build in.json  file.gxt      -> writes a GXT from the JSON
Layout: 6-byte header ("GBL" + version), then chunks {char[4] type; u32 size; data}:
  TKEY: entries {u32 offset_in_TDAT; char key[8]}, TDAT: UTF-16LE strings, 0-terminated.
Other chunks are kept verbatim."""
import json, struct, sys

def read(path):
    d = open(path, 'rb').read()
    header = d[:6]
    chunks, pos = [], 6
    while pos + 8 <= len(d):
        t = d[pos:pos+4].decode('latin1'); size = struct.unpack_from('<I', d, pos+4)[0]
        chunks.append((t, d[pos+8:pos+8+size])); pos += 8 + size
    return header, chunks

def dump(src, dst):
    header, chunks = read(src)
    cd = dict(chunks)
    tkey, tdat = cd['TKEY'], cd['TDAT']
    order, strings = [], {}
    for i in range(0, len(tkey), 12):
        off, key = struct.unpack_from('<I8s', tkey, i)
        key = key.split(b'\0')[0].decode('latin1')
        end = off
        while tdat[end:end+2] != b'\0\0':
            end += 2
        order.append(key); strings[key] = tdat[off:end].decode('utf-16-le')
    extra = {t: c.hex() for t, c in chunks if t not in ('TKEY', 'TDAT')}
    json.dump({'header': header.hex(), 'order': order, 'strings': strings, 'extra_chunks': extra,
               'chunk_order': [t for t, _ in chunks]}, open(dst, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(order), 'strings')

def build(src, dst):
    j = json.load(open(src, encoding='utf-8'))
    tdat = bytearray(); tkey = bytearray(); offs = {}
    for key in j['order']:
        offs[key] = len(tdat)
        tdat += j['strings'][key].encode('utf-16-le') + b'\0\0'
    # TKEY must stay sorted the way the original is (the game binary-searches it)
    for key in j['order']:
        tkey += struct.pack('<I8s', offs[key], key.encode('latin1'))
    out = bytearray(bytes.fromhex(j['header']))
    for t in j['chunk_order']:
        data = tkey if t == 'TKEY' else tdat if t == 'TDAT' else bytes.fromhex(j['extra_chunks'][t])
        out += t.encode('latin1') + struct.pack('<I', len(data)) + data
    open(dst, 'wb').write(out)
    print('wrote', dst, len(out), 'bytes')

if __name__ == '__main__':
    {'dump': dump, 'build': build}[sys.argv[1]](sys.argv[2], sys.argv[3])
