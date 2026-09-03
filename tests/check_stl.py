#!/usr/bin/env python3
import sys, struct
for path in sys.argv[1:]:
    data = open(path, "rb").read()
    n = struct.unpack("<I", data[80:84])[0]
    assert len(data) == 84 + 50 * n, (len(data), n)
    edges = {}
    vol = 0.0
    for i in range(n):
        rec = struct.unpack("<12fH", data[84 + 50 * i: 84 + 50 * (i + 1)])
        a, b, c = rec[3:6], rec[6:9], rec[9:12]
        vol += (a[0]*(b[1]*c[2]-b[2]*c[1]) - a[1]*(b[0]*c[2]-b[2]*c[0]) + a[2]*(b[0]*c[1]-b[1]*c[0]))
        for e in ((a, b), (b, c), (c, a)):
            edges[e] = edges.get(e, 0) + 1
    bad = sum(1 for (a, b), k in edges.items() if k != edges.get((b, a), 0))
    print(f"{path}: {n} triangles, volume {vol/6:.1f} mm^3, {'closed' if bad == 0 else 'OPEN (%d bad edges)' % bad}")
