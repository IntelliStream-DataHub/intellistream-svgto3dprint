#!/usr/bin/env python3
"""Compare a `logo3dprint --info` output with the recorded expectation.

Text must match exactly and so must integer counts (paths, shapes, slots,
pieces). Numbers with a decimal point (sizes, areas, volumes) may differ by
0.1% or 0.05, whichever is larger: the last digit of a tessellated area can
move between compilers and platforms.

Triangle counts are dropped before comparing.  How a polygon gets triangulated
turns on comparisons of intermediate doubles inside the tessellator and the
region booleans, so a host with a fused multiply-add reaches a different, and
equally correct, triangulation of the same shape.  The shape itself is what the
test is about, and that is still pinned: slots, areas, volumes, bounding box,
and - through check_stl.py - that every solid comes out closed."""
import re, sys

NUM = re.compile(r"-?\d+(?:\.\d+)?")
# "triangles: 26816" on its own line, "tris 524" on a slot, "236 tris" on the base
TRI_LINE = re.compile(r"^triangles: \d+$")
TRI_FIELD = re.compile(r"\s+(?:tris \d+|\d+ tris)\b")

def normalise(text):
    out = []
    for line in text.rstrip("\n").split("\n"):
        if TRI_LINE.match(line):
            continue
        out.append(TRI_FIELD.sub("", line))
    return out

def compare(expected, actual):
    el = normalise(expected)
    al = normalise(actual)
    if len(el) != len(al):
        return f"{len(el)} lines expected, {len(al)} found"
    for i, (e, a) in enumerate(zip(el, al), 1):
        et, en = NUM.split(e), NUM.findall(e)
        at, an = NUM.split(a), NUM.findall(a)
        if et != at or len(en) != len(an):
            return f"line {i} differs:\n  expected: {e}\n  actual:   {a}"
        for x, y in zip(en, an):
            if "." in x or "." in y:
                fx, fy = float(x), float(y)
                if abs(fx - fy) > max(0.05, 1e-3 * abs(fx)):
                    return f"line {i}: {x} expected, {y} found:\n  expected: {e}\n  actual:   {a}"
            elif x != y:
                return f"line {i}: {x} expected, {y} found:\n  expected: {e}\n  actual:   {a}"
    return None

if __name__ == "__main__":
    # `--normalise IN OUT` records an expectation: the triangle counts are
    # stripped so the file holds only what the comparison actually checks
    if sys.argv[1] == "--normalise":
        text = "\n".join(normalise(open(sys.argv[2]).read()))
        open(sys.argv[3], "w").write(text + "\n")
        sys.exit(0)
    exp, act = sys.argv[1], sys.argv[2]
    problem = compare(open(exp).read(), open(act).read())
    if problem:
        print(f"{act} vs {exp}: {problem}")
        sys.exit(1)
