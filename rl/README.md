# PiELo program-synthesis verifier

A verifiable benchmark for programs written in PiELo: a documented task format, a
semantic verifier over the PiELo toolchain, and sandbox hardening so a verified run
reproduces. **No ML yet** — this is the scaffold a code-generating model gets dropped
into next (running a model against the tasks and analyzing its failures is the next
step).

It is the generalization of `tests/run_tests.sh` from exact-golden-diff to a
**reference-as-oracle, randomized-trial** check. It reuses that harness's machinery
and its three correctness lessons (compile to an explicit `-o` path; fail loudly on
parser error / empty output; run offline under a wall-clock cap).

```
python3 rl/selftest.py                 # validate the CHECKER: refs pass, decoys fail
python3 rl/tasks.py                     # list tasks
python3 rl/tasks.py L3_react1 sol.txt   # verify a candidate body against a task
```

---

## What a task is

A **task** = an NL prompt (for a future model) + a machine checker + a reference
solution. In code it's a `Task` (`tasks.py`):

| field | meaning |
|---|---|
| `id`, `tier`, `prompt` | identity + the natural-language spec a model sees |
| `reference` | a correct solution **body** (a sequence of expressions, *no* outer `begin`). It is the **oracle** — expected output is whatever the reference prints, not hand-computed. |
| `decoys` | known-wrong programs (incl. reward-hacks) that **must fail**. The "a test that can't fail is worse than no test" net for the checker. |
| `make_trial(i, rng)` | returns `(prelude, postlude)` — harness-injected, **randomized** per trial |
| `n_trials`, `banned`, `mechanism` | trial count; banned C-builtins; an optional reactive dep to *report* (secondary, see below) |

### The I/O contract

A candidate emits a **body** (no outer `begin`). The harness compiles, per trial:

```
(begin
   {prelude}     ← harness: declares + sets the task's input vars (randomized)
   {candidate}   ← the model's answer
   {postlude}    ← harness: post-hoc interventions, e.g. randomized (set a …)
)
```

The prompt states the contract ("globals `a`,`b` are already declared and set"; "the
environment will then update `a` several times"). The reference and the candidate are
run under the **same** `(prelude, postlude)` each trial, and their **observable
outcomes** are compared. The outcome is the ordered list of `Stack top: <value>`
lines — the `PRINT` opcode's output — extracted with a regex that survives VM noise
(GC traces, `Ran end.`, and the stray unconditional `int N` arg-print leak from
`closureInstructions.cpp:60`).

---

## The five tasks (laddered, single-robot)

| id | tier | what it exercises |
|---|---|---|
| `L0_sum2` | logic | arithmetic over two random globals |
| `L1_max2` | logic | comparison + `if`; trials force tie / a>b / b>a |
| `L2_count_up` | loop | `while` printing `0..n-1`; trials force n=0/1/20 edges |
| `L3_react1` | **reactivity (1 dep)** | a reactive closure must re-run on post-hoc `set a` |
| `L4_react2` | **reactivity (2 deps)** | re-run on updates to **either** `a` or `b` |

Single-robot only, by design: offline mode is already deterministic, so these need
**no new VM work**. No robot-position / kinematics tasks (`updatePos` is wall-clock-based
and nondeterministic; those builtins are banned anyway).

---

## Anti-gaming (the crux)

The checker must verify the **mechanism**, not the surface output. The canonical hack
is the reactivity task answered by `(print 11)(print 12)(print 13)` with no reactivity
at all. The defense is layered; each layer is honest about what it does and doesn't do.

1. **Reference-as-oracle + K randomized trials (workhorse).** Inputs are
   harness-injected and randomized; the candidate must match the reference's outcome
   on *all* K trials. A constant-output hardcode survives one trial, not eight.

2. **Behavioral intervention for reactivity (the headline).** The values that
   determine the correct output (the post-hoc `set a` / `set b`) are injected by the
   harness **after** the candidate's code and randomized — *unknown at write time*.
   The only way to produce a tracking trace is a genuine reactive dependency. This
   turns "verify the mechanism" into "verify behavior under intervention." Validated:
   the `inert`-not-`reactive` decoy and the output-hardcode decoy both fail.

3. **Assembly-shape signal (secondary, NON-gating).** The verifier records whether the
   compiled candidate actually carries the expected reactive dependency
   (`Task.mechanism`), parsed from the `define_closure` line. It is **reported, never
   used to pass/fail** — it can be over/under-fit and a vestigial closure could fake
   it, so the behavioral trace match is always the gate.

4. **Decoy self-test (`selftest.py`).** Every task ships its known reward-hacks; the
   self-test asserts reference→pass and every decoy→fail. These decoys double as a
   catalogue of the reward-hacks this checker is known to resist.

**What this does NOT defeat:** reward-hacks not yet imagined. The intervention design
kills the *known* headline hack and instruments for unknowns; finding the unknowns is
an open research question, not something closed here.

---

## Sandbox / determinism

A single-robot offline run is deterministic given the program **except** for two
things, both controlled here:

- **`PIELO_SEED`** (new, `VM/main.cpp`): forces a deterministic `srand`, so
  `random_sleep` and any `rand()` use are repeatable. Unset → old wall-clock seed, so
  `make test` stays byte-identical. The verifier always sets it.
- **Banned C-builtins** (`go_forward`, `print_robot_pos`, `random_sleep`,
  `do_nothing`): the verifier scans the compiled assembly and **rejects** candidates
  that call them — this kills the `updatePos` wall-clock path and keeps batches fast.

Plus the inherited **4s per-run timeout** and `PIELO_OFFLINE=1`. Verified empirically:
the same composed program is byte-identical across runs.

---

## Verifier interface (`verify.py`)

```python
verify(task, candidate, *, seed=0, n_trials=None) -> Result   # pass iff ALL trials pass
verify_batch(task, [candidates], ...) -> [Result]             # pure per-candidate → parallelizable
```

`verify` is pure (isolated temp dirs, no shared state), so `verify_batch` parallelizes
unchanged — which batched generation needs. `Result` keeps the first failing
trial's composed inputs and both traces for debugging. Lower-level pieces
(`compile_to_asm`, `scan_banned`, `run_offline`, `extract_outcome`) are exported too.

---

## Status and limitations

This is **infrastructure, not a result.** It shows the VM-as-verifier shape is real,
the pipeline batches deterministically, and the headline reward-hack is defeated. It
does **not** show that anything *learns* — there is no model and no model evaluation
yet (that is the next step). Five tasks pins the *format*; it is not yet a broad
benchmark (tens to hundreds of tasks). And it is **single-robot only** — the
distinctive PiELo behavior (stigmergy / multi-robot coordination) is untouched, and
exercising it as a reward would first need deterministic multi-robot execution.

### Next

1. Put a model in the loop: run a code-generating model against the five tasks and
   analyze where and how it fails.
2. Broaden to tens–hundreds of tasks.
3. Coordination tasks, which first need deterministic multi-robot execution.
