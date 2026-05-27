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

ALL=(propagation:scenario_propagation barrier:scenario_barrier)

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
