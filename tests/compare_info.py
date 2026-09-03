#!/usr/bin/env python3
"""Compare a `logo3dprint --info` output with the recorded expectation.

Text must match exactly and so must integer counts (paths, shapes, slots,
pieces, triangles). Numbers with a decimal point (sizes, areas, volumes) may
differ by 0.1% or 0.05, whichever is larger: the last digit of a tessellated
area can move between compilers and platforms."""
import re, sys

NUM = re.compile(r"-?\d+(?:\.\d+)?")

def compare(expected, actual):
    el = expected.rstrip("\n").split("\n")
    al = actual.rstrip("\n").split("\n")
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
    exp, act = sys.argv[1], sys.argv[2]
    problem = compare(open(exp).read(), open(act).read())
    if problem:
        print(f"{act} vs {exp}: {problem}")
        sys.exit(1)
