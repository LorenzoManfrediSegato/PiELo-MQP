#!/usr/bin/env bash
#
# PiELo multi-robot (networked) regression harness.
#
# The networked twin of tests/run_tests.sh. Where the offline suite covers LOCAL
# behavior by golden-diffing deterministic single-robot runs, this suite exercises
# the NETWORK path -- the router, broadcast, and merge logic that the offline suite
# cannot reach. Because it is process-and-UDP based it asserts INVARIANTS
# ("every robot eventually prints X"), not exact transcripts.
#
# Determinism comes from PIELO_GO_AFTER=N (the router fires the go-signal exactly
# when N robots have registered -- no sleep-guessing, no stdin), plus deliberate
# launch ordering where a scenario needs it. Residual timing nondeterminism is
# absorbed by a generous convergence window and a small retry budget.
#
# Usage:
#   tests/run_multi_tests.sh            # build, run every scenario, report. Nonzero exit on failure.
#   tests/run_multi_tests.sh <name>     # run a single scenario by name
#
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM="$ROOT/VM/VM"
ROUTER="$ROOT/VM/router"
LOGDIR="$(mktemp -d "${TMPDIR:-/tmp}/pielo_multi.XXXXXX")"
RETRIES=2          # extra attempts if a scenario fails (absorbs rare UDP/timing flakes)

PIDS=()
track() { PIDS+=("$1"); }
reap() {
  for p in "${PIDS[@]:-}"; do kill "$p" 2>/dev/null; done
  PIDS=()
  pkill -f "$ROUTER" 2>/dev/null
  pkill -f "$VM " 2>/dev/null
}
trap 'reap; rm -rf "$LOGDIR"' EXIT

# start_router N logfile  -- router that auto-sends "go" once N clients register
start_router() {
  pkill -f "$ROUTER" 2>/dev/null; sleep 0.3
  PIELO_GO_AFTER="$1" "$ROUTER" >"$2" 2>&1 &
  track $!
}

# run_vm program logfile  -- one robot (NOT offline -> uses the router)
run_vm() {
  "$VM" "$ROOT/$1" >"$2" 2>&1 &
  track $!
}

# assert_all_contain marker logfile...  -- pass iff every log contains marker
assert_all_contain() {
  local marker="$1"; shift
  local ok=1
  for lg in "$@"; do
    if ! grep -q "$marker" "$lg"; then ok=0; fi
  done
  [ "$ok" = 1 ]
}

# Build VM + router from source so the suite never runs a stale binary (the
# networked analog of the Phase 0 stale-assembly.txt phantom-test bug). Tracing off
# (DEBUG_INSTRUCTIONS=0) for smaller logs; the markers are `debug_print`/raw cout, not
# gated by the trace flag, so grepping still works. A clean build is required because
# Make does not rebuild objects just because the -D define changed.
build() {
  echo "Building VM + router (tracing off)..."
  make -C "$ROOT/VM" clean >/dev/null 2>&1
  make -C "$ROOT/VM" DEBUG_INSTRUCTIONS=0 >/dev/null 2>&1 || { echo "VM build FAILED"; exit 2; }
}

# ----------------------------------------------------------------------------
# Scenarios. Each prints its own pass/fail detail and returns 0 (pass) / 1 (fail).
# ----------------------------------------------------------------------------

# Tagged propagation (cross-robot, non-reactive). Reader registers and settles
# FIRST (PIELO_GO_AFTER=1 fires go on its registration); writer late-joins and
# writes var=1 with a strictly-later timestamp, so the reader accepts cleanly.
# (Simultaneous writes would race -- see ROADMAP.md Phase 1.5 "Finding".)
scenario_propagation() {
  local r="$LOGDIR/prop_router.log" w="$LOGDIR/prop_waiter.log" u="$LOGDIR/prop_updater.log"
  start_router 1 "$r"; sleep 0.5
  run_vm VM/testPrograms/waiter.txt "$w"
  sleep 1.5                                   # reader registers, broadcasts its 0, begins polling
  run_vm VM/testPrograms/updater.txt "$u"
  sleep 3                                      # convergence margin
  reap
  if assert_all_contain "Loop done!" "$w"; then
    echo "  ok    propagation: waiter converged to var=1"
    return 0
  fi
  echo "  FAIL  propagation: waiter never printed 'Loop done!'"
  return 1
}

# Stigmergy merge (count). 5 robots start together (PIELO_GO_AFTER=5); each writes
# its own per-ID slot; a reactive closure re-counts on every received slot. Every
# robot must observe all 5 and print "Continuing!".
scenario_barrier() {
  local r="$LOGDIR/bar_router.log"; local logs=()
  start_router 5 "$r"; sleep 0.5
  for i in 1 2 3 4 5; do
    local lg="$LOGDIR/bar_$i.log"; logs+=("$lg")
    run_vm VM/testPrograms/barrier.txt "$lg"
  done
  sleep 8                                      # barrier.txt random_sleeps up to ~3s before writing
  reap
  if assert_all_contain "Continuing!" "${logs[@]}"; then
    echo "  ok    barrier: all 5 robots reached the barrier"
    return 0
  fi
  local n=0; for lg in "${logs[@]}"; do grep -q "Continuing!" "$lg" && n=$((n+1)); done
  echo "  FAIL  barrier: only $n/5 robots printed 'Continuing!'"
  return 1
}

# Cross-robot reactivity. Reader registers FIRST (PIELO_GO_AFTER=1 fires go on its
# registration), creates tagged x=0 and registers a reactive closure over it, then
# spins. Writer late-joins and broadcasts x=1 with a strictly-later timestamp; the
# reader's receive path (networking.cpp handleDependants) re-runs the closure, which
# prints "Reacted!". The closure prints ONLY when x!=0, so a stale echo of the reader's
# own x=0 cannot false-pass -- the marker means the network update actually drove a
# reactive rerun. (Single writer -> no contention; the local analog of this rerun is
# the offline parser_reactivity golden.)
scenario_reactivity() {
  local r="$LOGDIR/react_router.log" rd="$LOGDIR/react_reader.log" wr="$LOGDIR/react_writer.log"
  start_router 1 "$r"; sleep 0.5
  run_vm VM/testPrograms/reactive_reader.txt "$rd"
  sleep 1.5                                   # reader registers, creates x=0, registers watch, polls
  run_vm VM/testPrograms/reactive_writer.txt "$wr"
  sleep 3                                      # convergence margin
  reap
  if assert_all_contain "Reacted!" "$rd"; then
    echo "  ok    reactivity: reader's reactive closure re-ran on peer broadcast"
    return 0
  fi
  echo "  FAIL  reactivity: reader never printed 'Reacted!' (closure did not re-run on receive)"
  return 1
}

# Last-writer-wins. Reader registers FIRST (PIELO_GO_AFTER=1 fires go on its
# registration), creates tagged var=0, and watches it. Two writers late-join in strict
# order, ~1.5s apart: A writes var=1, then B writes var=2 with a later timestamp. The
# reader must FIRST observe A's value ("Saw A=1") and THEN converge to B's later value
# ("Converged to B=2") -- exactly the networking.cpp timestamp compare keeping the newer
# write. Requiring BOTH markers is what distinguishes this from plain propagation: it
# proves B's write superseded an already-established A, not merely that 2 arrived. A
# strictly-ordered pair, so no oscillation (ROADMAP Phase 1.5 "Finding"); the simultaneous
# case is a known bug, deliberately out of scope until the logical-clock work.
scenario_lww() {
  local r="$LOGDIR/lww_router.log" rd="$LOGDIR/lww_reader.log"
  local a="$LOGDIR/lww_writer_a.log" b="$LOGDIR/lww_writer_b.log"
  start_router 1 "$r"; sleep 0.5
  run_vm VM/testPrograms/lww_reader.txt "$rd"
  sleep 1.5                                   # reader registers, creates var=0, begins polling
  run_vm VM/testPrograms/lww_writer_a.txt "$a"
  sleep 1.5                                   # A's var=1 propagates and is observed before B writes
  run_vm VM/testPrograms/lww_writer_b.txt "$b"
  sleep 3                                      # convergence margin
  reap
  if assert_all_contain "Saw A=1" "$rd" && assert_all_contain "Converged to B=2" "$rd"; then
    echo "  ok    lww: reader saw A=1 then converged to B=2 (later write won)"
    return 0
  fi
  echo "  FAIL  lww: reader did not see A then converge to B ('Saw A=1' / 'Converged to B=2' missing)"
  return 1
}

# Retry wrapper: run a scenario up to 1+RETRIES times; pass on first success.
run_scenario() {
  local fn="$1" name="$2" attempt=1
  while :; do
    echo "scenario: $name (attempt $attempt)"
    if "$fn"; then return 0; fi
    [ "$attempt" -gt "$RETRIES" ] && return 1
    attempt=$((attempt+1)); sleep 1
  done
}

# ----------------------------------------------------------------------------

ALL=(propagation:scenario_propagation barrier:scenario_barrier reactivity:scenario_reactivity lww:scenario_lww)

build

filter="${1:-}"
pass=0; fail=0; failed=""
for entry in "${ALL[@]}"; do
  name="${entry%%:*}" fn="${entry#*:}"
  [ -n "$filter" ] && [ "$filter" != "$name" ] && continue
  if run_scenario "$fn" "$name"; then pass=$((pass+1)); else fail=$((fail+1)); failed="$failed $name"; fi
done

echo "------------------------------------------"
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ] || { echo "failures:$failed"; echo "logs kept on failure: $LOGDIR"; trap 'reap' EXIT; exit 1; }
echo "All multi-robot scenarios passed."
