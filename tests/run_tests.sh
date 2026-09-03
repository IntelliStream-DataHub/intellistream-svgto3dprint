#!/bin/sh
# Headless regression test: convert every example, validate the meshes.
set -e
BIN=${1:-./logo3dprint}
OUT=${TMPDIR:-/tmp}/logo3dprint-tests
mkdir -p "$OUT"
fail=0
for svg in examples/*.svg; do
    name=$(basename "$svg" .svg)
    "$BIN" --export "$OUT/$name.3mf" "$svg" > /dev/null
    "$BIN" --export "$OUT/$name.stl" --per-color "$svg" > /dev/null
    if command -v python3 > /dev/null 2>&1; then
        python3 tests/check_3mf.py "$OUT/$name.3mf" | tail -1 || fail=1
    fi
done
"$BIN" --info examples/simple.svg > /dev/null
exit $fail
