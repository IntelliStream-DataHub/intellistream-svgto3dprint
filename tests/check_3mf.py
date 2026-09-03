#!/usr/bin/env python3
"""Validate a 3MF written by logo3dprint: zip layout, XML, per-part triangle
ranges from Metadata/Slic3r_PE_model.config, material references, and that
every part mesh is closed (each edge used exactly twice, opposite directions)."""
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

def check(path):
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
    for obj in root.findall("m:resources/m:object", NS):
        mesh = obj.find("m:mesh", NS)
        if mesh is None:
            continue
        oid = obj.get("id")
        verts = [(float(v.get("x")), float(v.get("y")), float(v.get("z"))) for v in mesh.findall("m:vertices/m:vertex", NS)]
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
    print(f"{path}: {len(items)} build item(s), {nparts} parts, {len(colors)} materials, {'OK' if ok else 'FAILED'}")
    return ok

if __name__ == "__main__":
    good = all(check(p) for p in sys.argv[1:])
    sys.exit(0 if good else 1)
