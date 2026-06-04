"""
The task suite: hand-written single-robot tasks that pin the task FORMAT.

A task = (NL prompt for a future model) + (a machine checker) + (a reference
solution). The checker is reference-as-oracle over N randomized trials: the harness
wraps the same randomized (prelude, postlude) around both the candidate and the
reference, runs both offline, and compares observable outcomes (see verify.py).

Each task also ships `decoys`: known-wrong programs that MUST fail. They are the
"a test that can't fail is worse than no test" net for the *checker itself*
(selftest.py asserts reference->pass, every decoy->fail), and they double as a
catalogue of the reward-hacks this checker is known to resist.

I/O contract for a candidate body (no outer `begin`):
    (begin
       {prelude}    <- harness: declares + sets the task's input vars (randomized)
       {candidate}  <- the model's answer
       {postlude}   <- harness: post-hoc interventions, e.g. randomized (set a ...)
    )

Ladder: trivial logic -> branch -> loop -> 1-dep reactivity -> 2-dep reactivity.
No robot-position / kinematics tasks (updatePos is wall-clock-based and
nondeterministic), and those builtins are banned by the sandbox anyway.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from random import Random
from typing import Callable, List, Tuple

from verify import DEFAULT_BANNED


@dataclass
class Decoy:
    src: str
    why: str  # what wrongness / reward-hack this represents


@dataclass
class Task:
    id: str
    tier: str  # "logic" | "reactive"
    prompt: str
    reference: str
    decoys: List[Decoy]
    # (trial_index, rng) -> (prelude, postlude). The same call feeds both candidate
    # and reference, so they always see identical inputs.
    make_trial: Callable[[int, Random], Tuple[str, str]]
    n_trials: int = 8
    banned: Tuple[str, ...] = DEFAULT_BANNED
    # Optional secondary signal: a reactive dependency we EXPECT the compiled code to
    # carry. Recorded by the verifier, never used to gate (the behavioral trace match
    # is the gate). "" = no mechanism signal.
    mechanism: str = ""


# ---------------------------------------------------------------------------
# L0 -- arithmetic. Floor of the ladder, but already parameterized so anti-gaming
# is present from task one: a constant-output program survives one trial, not eight.
# ---------------------------------------------------------------------------

def _trial_sum2(i: int, rng: Random) -> Tuple[str, str]:
    a, b = rng.randint(0, 99), rng.randint(0, 99)
    prelude = f"(var global inert a)\n(set a {a})\n(var global inert b)\n(set b {b})"
    return prelude, ""


L0_sum2 = Task(
    id="L0_sum2",
    tier="logic",
    prompt=(
        "Two integer globals `a` and `b` are already declared and set to some "
        "values. Print their sum (use `print` so it reaches the stack-top output)."
    ),
    reference="(print (+ a b))",
    decoys=[
        Decoy("(print 7)", "constant-output hardcode; ignores the inputs"),
        Decoy("(print (- a b))", "wrong operator"),
        Decoy("(print a)", "ignores b"),
    ],
    make_trial=_trial_sum2,
)


# ---------------------------------------------------------------------------
# L1 -- branch. Adds a comparison + if. Trials include ties and both orderings.
# ---------------------------------------------------------------------------

def _trial_max2(i: int, rng: Random) -> Tuple[str, str]:
    # Deterministically cover all three orderings so the known decoys are always
    # exercised; the rest are random for anti-gaming against unknown candidates.
    a, b = rng.randint(0, 99), rng.randint(0, 99)
    if i == 0:        # tie
        b = a
    elif i == 1:      # a strictly greater
        a, b = max(a, b) + 1, min(a, b)
    elif i == 2:      # b strictly greater
        a, b = min(a, b), max(a, b) + 1
    prelude = f"(var global inert a)\n(set a {a})\n(var global inert b)\n(set b {b})"
    return prelude, ""


L1_max2 = Task(
    id="L1_max2",
    tier="logic",
    prompt=(
        "Two integer globals `a` and `b` are already declared and set. Print the "
        "larger of the two (if they are equal, print that value)."
    ),
    reference="(if (>= a b) (print a) (print b))",
    decoys=[
        Decoy("(print a)", "ignores the comparison; wrong when b > a"),
        Decoy("(print b)", "ignores the comparison; wrong when a > b"),
        Decoy("(if (< a b) (print a) (print b))", "comparison inverted -> prints the smaller"),
    ],
    make_trial=_trial_max2,
)


# ---------------------------------------------------------------------------
# L2 -- loop. The classic anti-hardcode case: a (print 0)..(print 9) hardcode dies
# the moment n != 10, and n=0 / n=1 edges are forced into the trial set.
# ---------------------------------------------------------------------------

def _trial_count_up(i: int, rng: Random) -> Tuple[str, str]:
    forced = {0: 0, 1: 1, 2: 20}  # cover the empty / single / max edges deterministically
    n = forced.get(i, rng.randint(0, 20))
    return f"(var global inert n)\n(set n {n})", ""


L2_count_up = Task(
    id="L2_count_up",
    tier="logic",
    prompt=(
        "An integer global `n` (0 <= n <= 20) is already declared and set. Print the "
        "integers 0, 1, 2, ..., n-1, one per print, in order. Print nothing if n is 0."
    ),
    reference=(
        "(var local inert i)\n"
        "(set i 0)\n"
        "(while (< i n) (begin (print i) (set i (+ 1 i))))"
    ),
    decoys=[
        Decoy(
            "(print 0)\n(print 1)\n(print 2)\n(print 3)\n(print 4)\n"
            "(print 5)\n(print 6)\n(print 7)\n(print 8)\n(print 9)",
            "output hardcoded to n=10; wrong for every other n (and for n=0)",
        ),
        Decoy(
            "(var local inert i)\n(set i 0)\n"
            "(while (<= i n) (begin (print i) (set i (+ 1 i))))",
            "off-by-one: <= prints 0..n instead of 0..n-1",
        ),
    ],
    make_trial=_trial_count_up,
)


# ---------------------------------------------------------------------------
# L3 -- reactivity (1 dependency). THE headline task. The values that determine the
# correct output (the post-hoc `set a` interventions) are injected by the harness
# AFTER the candidate's code and randomized, so they are unknown at write time.
# The only way to produce a tracking trace is a genuine reactive dependency on `a`.
# ---------------------------------------------------------------------------

def _trial_react1(i: int, rng: Random) -> Tuple[str, str]:
    a0 = rng.randint(0, 20)
    prelude = f"(var global reactive a)\n(set a {a0})"
    k = rng.randint(2, 4)
    postlude = "\n".join(f"(set a {rng.randint(0, 20)})" for _ in range(k))
    return prelude, postlude


L3_react1 = Task(
    id="L3_react1",
    tier="reactive",
    prompt=(
        "A reactive global `a` is already declared and set to an initial value. "
        "Define a REACTIVE function `f` of one argument `x` that prints (x + a), and "
        "call `f` once with 10 to register it. After your code runs, the environment "
        "will update `a` several times; `f` must automatically re-run and print the "
        "new (10 + a) each time. (Reactive references to a variable use the apostrophe "
        "suffix, e.g. `a'`.)"
    ),
    reference="(fun reactive f (x) (print (+ x a')))\n(f 10)",
    decoys=[
        Decoy(
            "(print 11)\n(print 12)\n(print 13)\n(print 14)",
            "output hardcode: matching surface output, ZERO reactivity (the canonical hack)",
        ),
        Decoy(
            "(fun inert f (x) (print (+ x a)))\n(f 10)",
            "inert, not reactive: prints once and never re-runs on the updates",
        ),
        Decoy(
            "(fun reactive f (x) (print x))\n(f 10)",
            "reactive on nothing: no dependency on a, never re-runs",
        ),
    ],
    make_trial=_trial_react1,
    mechanism="a",
)


# ---------------------------------------------------------------------------
# L4 -- reactivity (2 dependencies). Updates to EITHER `a` or `b` must re-run the
# closure, and the printed value must reflect both. A single-dependency solution
# misses half the interventions; a hardcode misses all of them.
# ---------------------------------------------------------------------------

def _trial_react2(i: int, rng: Random) -> Tuple[str, str]:
    a0, b0 = rng.randint(0, 20), rng.randint(0, 20)
    prelude = (
        f"(var global reactive a)\n(set a {a0})\n"
        f"(var global reactive b)\n(set b {b0})"
    )
    parts = []
    for _ in range(rng.randint(3, 5)):
        if rng.random() < 0.5:
            parts.append(f"(set a {rng.randint(0, 20)})")
        else:
            parts.append(f"(set b {rng.randint(0, 20)})")
    return prelude, "\n".join(parts)


L4_react2 = Task(
    id="L4_react2",
    tier="reactive",
    prompt=(
        "Two reactive globals `a` and `b` are already declared and set. Define a "
        "REACTIVE function `g` of one argument `x` that prints (x + a + b), and call "
        "`g` once with 100 to register it. After your code runs, the environment will "
        "update `a` and `b` several times; `g` must re-run and print the new "
        "(100 + a + b) after each update. Use apostrophe-suffixed reactive references "
        "(`a'`, `b'`)."
    ),
    reference="(fun reactive g (x) (print (+ x (+ a' b'))))\n(g 100)",
    decoys=[
        Decoy(
            "(fun reactive g (x) (print (+ x a')))\n(g 100)",
            "depends on a only: misses every b update (wrong value and wrong re-run count)",
        ),
        Decoy(
            "(print 100)\n(print 100)\n(print 100)\n(print 100)",
            "output hardcode; ignores both dependencies",
        ),
    ],
    make_trial=_trial_react2,
    mechanism="b",  # the dependency a single-dep cheat would drop
)


TASKS: List[Task] = [L0_sum2, L1_max2, L2_count_up, L3_react1, L4_react2]
TASKS_BY_ID = {t.id: t for t in TASKS}


# --- tiny CLI: list tasks, or verify one candidate file against a task ---------

if __name__ == "__main__":
    import sys
    from verify import verify

    if len(sys.argv) == 1:
        for t in TASKS:
            print(f"{t.id:14s} [{t.tier}]  {t.prompt[:70]}...")
        sys.exit(0)

    if len(sys.argv) != 3:
        print("usage: python3 tasks.py [<task_id> <candidate_file>]")
        sys.exit(2)

    task = TASKS_BY_ID.get(sys.argv[1])
    if task is None:
        print(f"unknown task '{sys.argv[1]}'. known: {', '.join(TASKS_BY_ID)}")
        sys.exit(2)
    with open(sys.argv[2]) as f:
        candidate = f.read()
    r = verify(task, candidate)
    print(f"{r.task_id}: {'PASS' if r.passed else 'FAIL'} -- {r.reason}")
    if not r.passed and r.first_failure:
        t = r.first_failure
        print(f"  prelude:  {t.prelude!r}")
        print(f"  postlude: {t.postlude!r}")
        print(f"  candidate trace:  {t.candidate_trace}")
        print(f"  reference trace:  {t.reference_trace}")
    sys.exit(0 if r.passed else 1)
