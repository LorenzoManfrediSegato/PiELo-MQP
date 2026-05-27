#!/usr/bin/env bash
#
# PiELo offline regression harness.
#
# Runs a curated set of single-robot programs in offline mode (PIELO_OFFLINE=1,
# no router) and diffs their output against committed golden files. Because it
# runs offline, this suite covers LOCAL behavior only: arithmetic, comparison /
# logic, closures, GC, control flow, local reactivity and local stigmergy.
#
# It does NOT cover cross-robot reactivity or multi-robot stigmergy merge -- that
# logic only runs on the network path. See tests/README.md for the manual
# multi-robot smoke tests that cover it.
#
# Usage:
#   tests/run_tests.sh           # build, run, diff against golden/. Nonzero exit on any mismatch.
#   tests/run_tests.sh --update  # regenerate the golden files (review the diff before committing!)
#
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOLDEN="$ROOT/tests/golden"
PARSER="$ROOT/Parser/parser"
VM="$ROOT/VM/VM"
TIMEOUT_S=4   # cap per run; programs ending in (spin) are expected to hit this

UPDATE=0
[ "${1:-}" = "--update" ] && UPDATE=1

mkdir -p "$GOLDEN"

# Assembly programs fed straight to the VM: "name=relative/path/to/assembly.txt"
VM_TESTS=(
  "vm_basicRegistration=VM/testPrograms/basicRegistration.txt"
  "vm_gctest=VM/testPrograms/gctest.txt"
  "vm_logicTest=VM/testPrograms/logicTest.txt"
  "vm_stigsize=VM/testPrograms/stigsize.txt"
  "vm_updater=VM/testPrograms/updater.txt"
)

# Source programs compiled by the parser, then run on the VM: "name=path/to/source.txt"
PARSER_TESTS=(
  "parser_iftest=Parser/testPrograms/iftest.txt"
  "parser_whiletest=Parser/testPrograms/whiletest.txt"
  "parser_reactivity=Parser/testPrograms/reactivitytest.txt"   # local reactivity
  "parser_stigtest=Parser/testPrograms/stigtest.txt"           # local stigmergy (spins; output captured up to timeout)
)

# Run argv under a wall-clock cap without depending on coreutils `timeout`.
run_capped() { perl -e 'alarm shift; exec @ARGV' "$TIMEOUT_S" "$@" 2>&1; }

build() {
  echo "Building parser and VM (tracing off)..."
  make -C "$ROOT/Parser" >/dev/null 2>&1 || { echo "parser build FAILED"; exit 2; }
  # Force a clean VM build: the golden files are captured with DEBUG_INSTRUCTIONS=0,
  # and Make does not rebuild objects just because the -D define changed, so a plain
  # `make DEBUG_INSTRUCTIONS=0` over a traced build would leave the trace output in.
  # (This leaves the VM built with tracing OFF; run `make -C VM` to restore tracing.)
  make -C "$ROOT/VM" clean >/dev/null 2>&1
  make -C "$ROOT/VM" DEBUG_INSTRUCTIONS=0 >/dev/null 2>&1 || { echo "VM build FAILED"; exit 2; }
}

# Produce a program's output. $1 = source/assembly path, $2 = "vm" or "parser".
capture() {
  local path="$ROOT/$1" kind="$2"
  if [ "$kind" = parser ]; then
    # Compile to an EXPLICIT output path with -o, then run THAT. The parser's
    # default output is 'assembly.txt' in the CWD; when the harness runs from the
    # repo root that is NOT $ROOT/Parser/assembly.txt, so every parser test used to
    # silently run a stale committed Parser/assembly.txt (all 4 parser goldens were
    # byte-identical). Failing to compile now emits a distinctive marker instead of
    # running a stale file, so a broken/missing source fails loudly.
    local asm="${TMPDIR:-/tmp}/pielo_offline_assembly.txt"
    rm -f "$asm"
    if ! "$PARSER" -o "$asm" "$path" >/dev/null 2>&1; then echo "PARSER-FAILED: $1"; return; fi
    if [ ! -f "$asm" ]; then echo "PARSER-NO-OUTPUT: $1"; return; fi
    PIELO_OFFLINE=1 run_capped "$VM" "$asm"
  else
    PIELO_OFFLINE=1 run_capped "$VM" "$path"
  fi
}

build

pass=0; fail=0; failed_names=""
run_suite() {
  local kind="$1"; shift
  for entry in "$@"; do
    local name="${entry%%=*}" path="${entry#*=}"
    local out; out="$(capture "$path" "$kind")"
    local gf="$GOLDEN/$name.expected"
    if [ "$UPDATE" = 1 ]; then
      printf '%s\n' "$out" > "$gf"
      echo "updated  $name"
    elif [ ! -f "$gf" ]; then
      echo "MISSING  $name (no golden file; run --update)"; fail=$((fail+1)); failed_names="$failed_names $name"
    elif [ "$out" = "$(cat "$gf")" ]; then
      echo "ok       $name"; pass=$((pass+1))
    else
      echo "FAIL     $name"; fail=$((fail+1)); failed_names="$failed_names $name"
      diff <(cat "$gf") <(printf '%s\n' "$out") | sed 's/^/           /' | head -20
    fi
  done
}

run_suite vm "${VM_TESTS[@]}"
run_suite parser "${PARSER_TESTS[@]}"

echo "------------------------------------------"
if [ "$UPDATE" = 1 ]; then
  echo "Golden files regenerated in $GOLDEN. Review with 'git diff' before committing."
  exit 0
fi
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ] || { echo "failures:$failed_names"; exit 1; }
echo "All offline regression tests passed."
