# PiELo tests

## Offline regression suite (automated)

```bash
tests/run_tests.sh            # build, run, diff against tests/golden/  (exit 1 on any mismatch)
tests/run_tests.sh --update   # regenerate golden files after an intentional output change
```

The suite builds the parser and VM, then runs a curated set of **single-robot**
programs with `PIELO_OFFLINE=1` (see below) and diffs their combined
stdout/stderr against the committed files in `tests/golden/`. Everything runs
without a router, so it is deterministic and needs no network setup.

### What it covers

| Area | Tests |
|------|-------|
| Arithmetic / comparison / logic | `vm_logicTest` |
| Closures + garbage collection | `vm_gctest`, `vm_basicRegistration` |
| Control flow (`if`, `while`) | `parser_iftest`, `parser_whiletest` |
| **Local** reactivity (a robot's own dependants re-running on store) | `parser_reactivity` |
| **Local** stigmergy (own slot + `foreach`) | `vm_stigsize`, `parser_stigtest` |
| Tagged-store bookkeeping (no broadcast) | `vm_updater` |

This is the surface the Phase 4 refactors (arithmetic/comparison dedup, the
`VariableData`/closure memory model, GC) touch, so the suite is the safety net
for those changes.

### What it does NOT cover — read this before trusting a green run

`PIELO_OFFLINE=1` makes `initNetworking`, `checkForMessage`, and
`broadcastVariable` no-ops. That means **anything that depends on a message
arriving from another robot is never exercised**:

- **Cross-robot reactivity** — a `tagged` reactive variable re-running its
  dependants when a *peer* broadcasts a new value. (Local reactivity, where a
  robot stores to its own variable, *is* covered by `parser_reactivity`.)
- **Multi-robot stigmergy merge** — a robot folding *other* robots' slots into
  its stigmergy map, and `foreach` then iterating across the merged set. (A
  single robot's own slot *is* covered.)
- **Last-writer-wins conflict resolution** — the timestamp comparison in
  `checkForMessage` only runs when two robots write the same tagged variable.
- Registration / go-signal handshake and the broadcast *send* path.

These only happen on the network path. **A green offline run says nothing about
them** — they must be checked with the manual multi-robot smoke tests below, and
should be the first thing re-verified after touching `networking.cpp`,
`storeLoad.cpp`'s broadcast calls, or `handleDependants`.

## Manual multi-robot smoke tests

Run **without** `PIELO_OFFLINE` so the real networking path is exercised. Build
with `make -C VM` first. Each needs the router plus one or more VM processes;
press Enter in the router terminal to broadcast the go-signal once all robots
have registered.

### Cross-robot reactivity / tagged-variable propagation

`waiter` spins on a tagged `var` until it becomes nonzero; `updater` sets `var=1`
and broadcasts. Build with `DEBUG_INSTRUCTIONS=1` to see the trace.

```bash
./VM/router                          # terminal 1
./VM/VM VM/testPrograms/waiter.txt   # terminal 2 (registers, then loops)
./VM/VM VM/testPrograms/updater.txt  # terminal 3
# press Enter in terminal 1
```

**Expected:** after the go-signal, `waiter` receives `updater`'s broadcast,
escapes its loop, and prints `Loop done!`. Offline, `waiter` loops forever — so
this is exactly the behavior the offline suite cannot see.

### Multi-robot stigmergy merge

`barrier` waits until 5 robots have written the stigmergy `barrier` slot, then
calls `go_forward`.

```bash
./VM/router                                  # terminal 1
for i in 1 2 3 4 5; do ./VM/VM VM/testPrograms/barrier.txt & done   # 5 robots
# press Enter in terminal 1
```

**Expected:** once all 5 slots are present, each robot prints `Continuing!` and
goes forward. Validates that peers' stigmergy slots are merged and counted.

## Notes for adding/maintaining tests

- Golden files are combined stdout+stderr captured in offline mode with
  `DEBUG_INSTRUCTIONS=0`. Build trace output (`DEBUG_INSTRUCTIONS=1`) is **not**
  part of the golden output; keep the suite building with tracing off.
- Programs ending in `(spin)` never terminate; the runner caps each run at
  `TIMEOUT_S` seconds and compares whatever was printed before the cap. Only add
  a spin program if it goes quiet after emitting its output (no printing inside
  the spin loop), otherwise capture is racy.
- Several existing programs are intentionally excluded: ones using
  `random_sleep` or `print_robot_pos` are nondeterministic (wall-clock based),
  and a few (`closure_test`, `simple_closure_test`, `stigMath*`, `inputtest`)
  currently crash on stale assembly syntax the VM parser no longer accepts.
