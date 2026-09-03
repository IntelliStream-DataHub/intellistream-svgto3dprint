#!/usr/bin/env python3
"""Validate a 3MF written by logo3dprint: zip layout, XML, per-part triangle
ranges from Metadata/Slic3r_PE_model.config, material references, and that
every part mesh is closed (each edge used exactly twice, opposite directions).

    check_3mf.py [--bed WxD] FILE...

--bed WxD: a file exported per printer plate; every build item must lie on
the W x D mm plate (origin at its front-left corner) and no two may overlap."""
import sys, zipfile, xml.etree.ElementTree as ET
from collections import Counter

NS = {"m": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02",
      "mat": "http://schemas.microsoft.com/3dmanufacturing/material/2015/02"}

def closed(verts, tris):
    edges = Counter()
    vol = 0.0
    for a, b, c in tris:
        for e in ((a, b), (b, c), (c, a)):
            edges[e] += 1
        A, B, C = verts[a], verts[b], verts[c]
        vol += (A[0]*(B[1]*C[2]-B[2]*C[1]) - A[1]*(B[0]*C[2]-B[2]*C[0]) + A[2]*(B[0]*C[1]-B[1]*C[0]))
    # an edge must be matched by the same number of reversed edges; balanced
    # multi-use (two shells touching along an edge) is a pinch, not a hole
    bad = sum(1 for (a, b), n in edges.items() if n != edges.get((b, a), 0))
    return bad, vol / 6.0

def item_bbox(root, it, bboxes):
    """XY bounding box of a build item (translation-only transform)."""
    tf = [float(v) for v in it.get("transform", "1 0 0 0 1 0 0 0 1 0 0 0").split()]
    assert tf[:9] == [1, 0, 0, 0, 1, 0, 0, 0, 1], "build items must be translations only"
    asm = root.find(f"m:resources/m:object[@id='{it.get('objectid')}']", NS)
    bb = [1e300, 1e300, -1e300, -1e300]
    for c in asm.findall("m:components/m:component", NS):
        b = bboxes[c.get("objectid")]
        bb = [min(bb[0], b[0]), min(bb[1], b[1]), max(bb[2], b[2]), max(bb[3], b[3])]
    return [bb[0] + tf[9], bb[1] + tf[10], bb[2] + tf[9], bb[3] + tf[10]]

def no_overlap(items_bb):
    for i in range(len(items_bb)):
        for j in range(i + 1, len(items_bb)):
            a, b = items_bb[i][1], items_bb[j][1]
            if a[0] < b[2] - 1e-6 and b[0] < a[2] - 1e-6 and a[1] < b[3] - 1e-6 and b[1] < a[3] - 1e-6:
                raise AssertionError(f"objects {items_bb[i][0]} and {items_bb[j][0]} overlap: {a} vs {b}")

def check_bed(root, items, bboxes, W, D):
    bbs = [(it.get("objectid"), item_bbox(root, it, bboxes)) for it in items]
    for oid, bb in bbs:
        assert bb[0] >= -1e-3 and bb[1] >= -1e-3 and bb[2] <= W + 1e-3 and bb[3] <= D + 1e-3, f"object {oid} bbox {bb} leaves the {W} x {D} plate"
    no_overlap(bbs)
    print(f"  {len(items)} piece(s) on the {W:g} x {D:g} mm plate, none overlapping")

def check(path, bed=None):
    z = zipfile.ZipFile(path)
    names = z.namelist()
    for req in ("[Content_Types].xml", "_rels/.rels", "3D/3dmodel.model", "Metadata/Slic3r_PE_model.config"):
        assert req in names, f"missing {req}"
    root = ET.fromstring(z.read("3D/3dmodel.model"))
    cfg = ET.fromstring(z.read("Metadata/Slic3r_PE_model.config"))
    bbs = ET.fromstring(z.read("Metadata/model_settings.config"))
    # every assembly must have a model_settings entry with one part per component
    for obj in root.findall("m:resources/m:object", NS):
        comps = obj.findall("m:components/m:component", NS)
        if not comps:
            continue
        bo = bbs.find(f"object[@id='{obj.get('id')}']")
        assert bo is not None, f"no model_settings for assembly {obj.get('id')}"
        part_ids = [pt.get("id") for pt in bo.findall("part")]
        assert part_ids == [c.get("objectid") for c in comps], (part_ids, [c.get("objectid") for c in comps])
        for pt in bo.findall("part"):
            assert pt.find("metadata[@key='extruder']") is not None
    colors = [c.get("color") for c in root.findall(".//mat:colorgroup/mat:color", NS)]
    bases = [b.get("displaycolor") for b in root.findall(".//m:basematerials/m:base", NS)]
    assert colors and colors == bases, (colors, bases)
    ok = True
    nparts = 0
    bboxes = {}
    for obj in root.findall("m:resources/m:object", NS):
        mesh = obj.find("m:mesh", NS)
        if mesh is None:
            continue
        oid = obj.get("id")
        verts = [(float(v.get("x")), float(v.get("y")), float(v.get("z"))) for v in mesh.findall("m:vertices/m:vertex", NS)]
        bboxes[oid] = (min(v[0] for v in verts), min(v[1] for v in verts), max(v[0] for v in verts), max(v[1] for v in verts))
        tris = []
        tri_mat = []
        for t in mesh.findall("m:triangles/m:triangle", NS):
            tris.append((int(t.get("v1")), int(t.get("v2")), int(t.get("v3"))))
            assert t.get("pid") == "2", "triangle must reference the colour group"
            tri_mat.append(int(t.get("p1")))
        cobj = cfg.find(f"object[@id='{oid}']")
        assert cobj is not None, f"no config for object {oid}"
        obj_meta = {md.get("key"): md.get("value") for md in cobj.findall("metadata")}
        vols = cobj.findall("volume")
        covered = 0
        for v in vols:
            f, l = int(v.get("firstid")), int(v.get("lastid"))
            covered += l - f + 1
            meta = {md.get("key"): md.get("value") for md in v.findall("metadata")}
            ext = int(meta["extruder"])
            mats = set(tri_mat[f:l + 1])
            assert mats == {ext - 1}, f"volume {meta.get('name')}: triangles reference materials {mats}, extruder {ext}"
            if "extruder" in obj_meta:
                assert int(obj_meta["extruder"]) == ext, "object extruder differs from its volume"
            bad, vol = closed(verts, tris[f:l + 1])
            status = "closed" if bad == 0 else f"OPEN ({bad} bad edges)"
            if bad:
                ok = False
            print(f"  object {oid} '{obj.get('name')}' part '{meta.get('name')}': tris {f}-{l}, extruder {ext}, volume {vol:.1f} mm^3, {status}")
            nparts += 1
        assert covered == len(tris), f"object {oid}: volumes cover {covered} of {len(tris)} triangles"
    items = root.findall("m:build/m:item", NS)
    for it in items:
        target = root.find(f"m:resources/m:object[@id='{it.get('objectid')}']", NS)
        assert target is not None and target.find("m:components", NS) is not None, "build items must reference assemblies"
    if bed:
        check_bed(root, items, bboxes, bed[0], bed[1])
    print(f"{path}: {len(items)} build item(s), {nparts} parts, {len(colors)} materials, {'OK' if ok else 'FAILED'}")
    return ok

if __name__ == "__main__":
    args = sys.argv[1:]
    bed = None
    while args and args[0].startswith("--"):
        if args[0] == "--bed":
            w, d = args[1].lower().split("x")
            bed = (float(w), float(d))
            args = args[2:]
        else:
            sys.exit(f"unknown option {args[0]}")
    results = [check(p, bed) for p in args]
    sys.exit(0 if all(results) else 1)
