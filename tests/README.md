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
them** — they are covered by the automated multi-robot suite below (`make
test-multi`), which should be the first thing re-verified after touching
`networking.cpp`, `storeLoad.cpp`'s broadcast calls, or `handleDependants`.

## Multi-robot regression suite (automated)

```bash
make test-multi                      # build + run every networked scenario (exit 1 on failure)
bash tests/run_multi_tests.sh        # same thing
bash tests/run_multi_tests.sh lww    # run a single scenario by name
```

The networked twin of the offline suite. It runs **without** `PIELO_OFFLINE` so
the real network path is exercised: it starts the `router` plus several `VM`
processes over UDP and asserts **invariants** ("every robot eventually prints
X"), not exact transcripts — process/UDP timing makes a byte-for-byte golden
impossible. It is **opt-in**: deliberately kept out of the fast `make test`
because it takes several seconds of real wall-clock per scenario.

Determinism comes from **`PIELO_GO_AFTER=N`** (test-only, in `router.cpp`): the
router fires the go-signal the instant the Nth client registers — no sleep
guessing, no stdin — plus deliberate launch ordering where a scenario needs a
writer sequenced after a reader. Residual timing slack is absorbed by generous
convergence windows and a small retry budget. The harness builds its own VM +
router (tracing off) each run, so it never tests a stale binary.

### What it covers

| Scenario | Invariant | Programs |
|---|---|---|
| `propagation` | a waiter spinning on a tagged var converges to a peer's broadcast → `Loop done!` | `waiter` + `updater` |
| `barrier` | 5 robots each write their own stigmergy slot; every robot merges all 5 and counts them → `Continuing!` | `barrier` ×5 |
| `reactivity` | a reactive closure over a **tagged** var re-runs on a peer's broadcast (the `handleDependants` call on the receive path) → `Reacted!` | `reactive_reader` + `reactive_writer` |
| `lww` | two writers sequenced by launch order; the reader observes A's value, then **converges to B's later-timestamped write** (the timestamp compare in `checkForMessage`) → `Saw A=1` **and** `Converged to B=2` | `lww_reader` + `lww_writer_a` + `lww_writer_b` |

Together these cover the four network behaviors the offline suite structurally
cannot: tagged propagation, stigmergy merge across peers, cross-robot reactive
recompute, and last-writer-wins conflict resolution.

**Caveat — tagged-var contention is out of scope.** Each scenario sequences a
single winning writer (or uses the merge-based, contention-immune stigmergy
path). Two *simultaneous* writers to the same tagged var do **not** converge today
— wall-clock LWW picks an arbitrary winner, and a rejected write is rebroadcast
with a fresh timestamp, so contending values oscillate. This is a known bug,
deliberately deferred to the logical-clock work (see `docs/ROADMAP.md` Phase 1.5
"Finding" and `docs/RL_DESIGN.md`); the `lww` scenario tests only the
strictly-ordered case, which converges cleanly.

### Running a scenario by hand (debugging)

The automated suite handles the go-signal for you. To drive one manually instead,
build with tracing on (`make -C VM`, or `DEBUG_INSTRUCTIONS=1` for per-opcode
output), start the router, launch the VMs, and press Enter in the router terminal
to broadcast the go-signal once all robots have registered:

```bash
./VM/router                          # terminal 1
./VM/VM VM/testPrograms/waiter.txt   # terminal 2 (registers, then loops)
./VM/VM VM/testPrograms/updater.txt  # terminal 3
# press Enter in terminal 1 -> waiter receives var=1 and prints "Loop done!"
```

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
