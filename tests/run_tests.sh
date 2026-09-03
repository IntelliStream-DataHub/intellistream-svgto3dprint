#!/bin/sh
# Headless regression tests for the command line.
#
# Every example SVG is converted in the main modes (plain, split into
# objects and tiles, side-by-side colours, no base, ...). Every exported
# mesh is validated: closed shells, 3MF part ranges, materials and
# extruders; and the `--info` statistics (sizes, areas, volumes, triangle
# counts, pieces) are compared with the recordings in tests/expected/, so
# an unintended change in the geometry shows up as a diff.
#
#   sh tests/run_tests.sh ./logo3dprint            run everything
#   UPDATE=1 sh tests/run_tests.sh ./logo3dprint   re-record tests/expected/
#                                                  after a deliberate change
#
# The mesh checks and the tolerant comparison need python3; without it only
# the conversions themselves and exact comparisons run.
set -u
BIN=${1:-./logo3dprint}
case "$BIN" in /*) ;; *) BIN=$(pwd)/$BIN ;; esac
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
EX=$ROOT/examples
EXPECTED=$HERE/expected
OUT=${TMPDIR:-/tmp}/logo3dprint-tests
rm -rf "$OUT"
mkdir -p "$OUT" "$EXPECTED"
PY=
command -v python3 > /dev/null 2>&1 && PY=python3
fail=0

fails() { echo "FAIL: $*"; fail=1; }

# check3mf FILE...   check every file that exists (CHECK3MF_ARGS: checker options)
CHECK3MF_ARGS=
check3mf() {
    [ -n "$PY" ] || return 0
    for f in "$@"; do
        [ -e "$f" ] || continue
        if res=$("$PY" "$HERE/check_3mf.py" $CHECK3MF_ARGS "$f" 2>&1); then
            echo "$res" | tail -1
        else
            echo "$res"
            fails "$f"
        fi
    done
}

checkstl() {
    [ -n "$PY" ] || return 0
    for f in "$@"; do
        [ -e "$f" ] || continue
        if res=$("$PY" "$HERE/check_stl.py" "$f" 2>&1); then
            echo "$res"
        else
            echo "$res"
            fails "$f"
        fi
    done
}

# info NAME ARGS...   run --info and compare with tests/expected/NAME.info
info() {
    name=$1
    shift
    if ! "$BIN" --info "$@" > "$OUT/$name.raw" 2>&1; then
        cat "$OUT/$name.raw"
        fails "--info $*"
        return
    fi
    # the first line names the input file with a path; keep the basename only
    sed 's|^file: .*/|file: |' "$OUT/$name.raw" > "$OUT/$name.info"
    if [ -n "${UPDATE:-}" ]; then
        cp "$OUT/$name.info" "$EXPECTED/$name.info"
        echo "recorded tests/expected/$name.info"
        return
    fi
    if [ ! -f "$EXPECTED/$name.info" ]; then
        fails "no tests/expected/$name.info (run with UPDATE=1 to record it)"
        return
    fi
    if [ -n "$PY" ]; then
        "$PY" "$HERE/compare_info.py" "$EXPECTED/$name.info" "$OUT/$name.info" || fails "$name: --info differs from tests/expected/$name.info"
    else
        cmp -s "$EXPECTED/$name.info" "$OUT/$name.info" || { diff "$EXPECTED/$name.info" "$OUT/$name.info"; fails "$name: --info differs from tests/expected/$name.info"; }
    fi
}

# convert NAME ARGS...   export 3MF and merged STL, validate every file written
convert() {
    name=$1
    shift
    "$BIN" --export "$OUT/$name.3mf" "$@" > /dev/null || fails "--export $name.3mf $*"
    "$BIN" --export "$OUT/$name.stl" "$@" > /dev/null || fails "--export $name.stl $*"
    check3mf "$OUT/$name.3mf" "$OUT/${name}_"*.3mf
    checkstl "$OUT/$name.stl" "$OUT/${name}_"*.stl
}

# run NAME ARGS...   both of the above
run() {
    info "$@"
    convert "$@"
}

# --- every example in its default form, plus one STL per colour -----------
for svg in "$EX"/*.svg; do
    name=$(basename "$svg" .svg)
    run "$name" "$svg"
    "$BIN" --export "$OUT/$name-percolor.stl" --per-color "$svg" > /dev/null || fails "--per-color $name"
    checkstl "$OUT/$name-percolor_"*.stl
done

# --- sizing and layout options ---------------------------------------------
run simple-height       --height 80 --mirror "$EX/simple.svg"
run simple-sidebyside   --no-layered --stagger 1,0.5 "$EX/simple.svg"
run simple-flush        --flush --base 1 --margin 5 --radius 0 "$EX/simple.svg"
run simple-nobase       --no-base "$EX/simple.svg"
run simple-body         --body 2 --body-height 3 --hide 3 --base-slot 1 "$EX/simple.svg"
run many-merge          --merge 60 --max-colors 4 "$EX/many_colors.svg"
run simple-fit          --fit-plate --plate 100x100 "$EX/simple.svg"
run simple-tolerance    --tolerance 0.2 "$EX/simple.svg"

# --- splitting into pieces --------------------------------------------------
run simple-tiles        --split tiles --plate 60x60 "$EX/simple.svg"
run simple-tiles-loose  --split tiles --plate 60x60 --no-joints "$EX/simple.svg"
run simple-tiles-keep   --split tiles --plate 60x60 --oversize keep "$EX/simple.svg"
run simple-tiles-fit    --split tiles --plate 120x120 --fit-plate "$EX/simple.svg"
run many-objects        --split objects "$EX/many_colors.svg"
run many-objects-tiles  --split objects --plate 60x60 --oversize cut "$EX/many_colors.svg"
run many-objects-each   --split objects --plate 60x60 --oversize each --join 0 "$EX/many_colors.svg"
run clip-tiles          --split tiles --plate 80x80 "$EX/clip_pattern.svg"
# every piece in one 3MF (assembled layout)
"$BIN" --export "$OUT/simple-onefile.3mf" --split tiles --plate 60x60 --single-file "$EX/simple.svg" > /dev/null || fails "--single-file"
check3mf "$OUT/simple-onefile.3mf"
"$BIN" --export "$OUT/arcs-onefile.3mf" --split objects --join 0 --plate 100x80 --single-file "$EX/evenodd_arcs.svg" > /dev/null || fails "--single-file objects"
check3mf "$OUT/arcs-onefile.3mf"

# --- pieces arranged on printer plates ---------------------------------------
# one file per plate: every piece on its plate, none overlapping (3MF and STL)
info many-plates --split objects --plate 60x60 --oversize cut --spacing 5 "$EX/many_colors.svg"
"$BIN" --export "$OUT/many-plates.3mf" --per-plate --split objects --plate 60x60 --oversize cut --spacing 5 "$EX/many_colors.svg" > /dev/null || fails "--per-plate 3mf"
CHECK3MF_ARGS="--bed 60x60" check3mf "$OUT/many-plates_plate"*.3mf
"$BIN" --export "$OUT/many-plates.stl" --per-plate --split objects --plate 60x60 --oversize cut --spacing 5 "$EX/many_colors.svg" > /dev/null || fails "--per-plate stl"
checkstl "$OUT/many-plates_plate"*.stl
info arcs-plates --split objects --join 0 --plate 100x80 "$EX/evenodd_arcs.svg"
"$BIN" --export "$OUT/arcs-plates.3mf" --per-plate --split objects --join 0 --plate 100x80 "$EX/evenodd_arcs.svg" > /dev/null || fails "--per-plate arcs"
CHECK3MF_ARGS="--bed 100x80" check3mf "$OUT/arcs-plates.3mf" "$OUT/arcs-plates_plate"*.3mf
# --- command line behaviour -------------------------------------------------
"$BIN" --help > /dev/null || fails "--help should exit 0"
"$BIN" --bogus "$EX/simple.svg" > /dev/null 2>&1 && fails "unknown option should fail"
"$BIN" --info "$OUT/does-not-exist.svg" > /dev/null 2>&1 && fails "missing input should fail"
"$BIN" --export "$OUT/x.3mf" > /dev/null 2>&1 && fails "--export without an SVG should fail"

if [ "$fail" -eq 0 ]; then
    echo "all CLI tests passed"
else
    echo "CLI tests FAILED"
fi
exit $fail
