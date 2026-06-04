"""
PiELo semantic verifier.

Given a task and a candidate PiELo *source body*, this compiles the program with
the Parser, runs it offline+deterministically on the VM under a timeout, extracts
the program's observable outcome from stdout, and returns pass/fail.

It is the generalization of `tests/run_tests.sh` from exact-golden-diff to a
semantic, reference-oracle check, and it preserves that harness's three hard-won
correctness lessons:

  1. compile to an EXPLICIT `-o` path (never run a stale assembly.txt),
  2. fail LOUDLY if the parser errors or produces no output,
  3. run offline under a wall-clock cap.

Determinism (so a verified run reproduces):
  - PIELO_OFFLINE=1  -> no router, robotID=0, broadcast/recv are no-ops.
  - PIELO_SEED=<n>   -> deterministic srand (VM/main.cpp), so random_sleep / any
                        rand() use is repeatable.
  - banned-builtin scan -> reject programs that call the wall-clock kinematic /
                        sleep C-builtins (the only remaining nondeterminism in a
                        single-robot offline run).

The verifier is PURE per candidate (no shared mutable state, isolated temp dirs),
so `verify_batch` is trivially parallelizable -- which batched generation needs.
"""

from __future__ import annotations

import os
import re
import subprocess
import tempfile
from dataclasses import dataclass, field
from random import Random
from typing import Callable, List, Optional, Tuple

# --- repo layout -------------------------------------------------------------

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PARSER = os.path.join(ROOT, "Parser", "parser")
VM = os.path.join(ROOT, "VM", "VM")

DEFAULT_TIMEOUT_S = 4  # same cap as tests/run_tests.sh
DEFAULT_SEED = 0

# C builtins whose only effect is wall-clock-based nondeterminism (kinematics via
# updatePos(), or a randomized sleep). A single-robot logic task never needs them;
# banning them keeps a verified run deterministic AND keeps batches fast (no usleep).
DEFAULT_BANNED = ("go_forward", "print_robot_pos", "random_sleep", "do_nothing")

# The PRINT opcode emits exactly "Stack top: <value>" -- that line IS the program's
# observable outcome. Everything else on VM stdout (GC traces, "Ran end.", "Done!",
# "Offline mode", and the stray unconditional "int N" arg-print leak from
# closureInstructions.cpp:60) is noise. Matching from "Stack top:" to end-of-line
# survives that leak: "int 10Stack top: 15" still yields "15".
_STACK_TOP = re.compile(r"Stack top: (.*)")
# A model may wrap its answer in markdown fences; strip them so candidates are usable
# straight from an API response.
_FENCE = re.compile(r"^\s*```[a-zA-Z]*\n(.*?)\n```\s*$", re.DOTALL)


class CompileError(RuntimeError):
    """The Parser rejected the program (nonzero exit or empty output)."""


# --- result types ------------------------------------------------------------


@dataclass
class TrialResult:
    index: int
    passed: bool
    reason: str
    prelude: str = ""
    postlude: str = ""
    candidate_trace: Optional[List[str]] = None
    reference_trace: Optional[List[str]] = None
    # Secondary, NON-gating signal: did the compiled candidate actually carry the
    # reactive dependency the task is about? Recorded for analysis, never used to
    # pass/fail -- the behavioral trace match is the gate.
    mechanism_present: Optional[bool] = None


@dataclass
class Result:
    task_id: str
    passed: bool
    reason: str
    trials: List[TrialResult] = field(default_factory=list)

    @property
    def first_failure(self) -> Optional[TrialResult]:
        return next((t for t in self.trials if not t.passed), None)


# --- toolchain primitives ----------------------------------------------------


def strip_code_fence(text: str) -> str:
    """Strip a single surrounding markdown code fence, if present."""
    m = _FENCE.match(text)
    return m.group(1) if m else text


def compose(prelude: str, body: str, postlude: str) -> str:
    """Wrap the harness prelude/postlude around the candidate body in one (begin)."""
    parts = [p for p in (prelude.strip(), body.strip(), postlude.strip()) if p]
    return "(begin\n" + "\n".join(parts) + "\n)\n"


def compile_to_asm(source: str, workdir: str) -> str:
    """Compile source -> assembly text. Raise CompileError on failure/empty output.

    Runs the parser with cwd=workdir so its intermediate `tmp_N` files (and any
    stray default output) stay isolated -- which is what makes parallel batching
    safe. The parser's chatty stdout is discarded; only success + the -o file matter.
    """
    src_path = os.path.join(workdir, "source.txt")
    asm_path = os.path.join(workdir, "out.asm")
    with open(src_path, "w") as f:
        f.write(source)
    try:
        proc = subprocess.run(
            [PARSER, "-o", asm_path, src_path],
            cwd=workdir,
            capture_output=True,
            text=True,
            timeout=DEFAULT_TIMEOUT_S,
        )
    except subprocess.TimeoutExpired:
        raise CompileError("parser timed out")
    if proc.returncode != 0:
        raise CompileError(f"parser exit {proc.returncode}: {proc.stderr.strip()[-300:]}")
    if not os.path.exists(asm_path) or os.path.getsize(asm_path) == 0:
        raise CompileError("parser produced no output")
    with open(asm_path) as f:
        return f.read()


def scan_banned(asm: str, banned=DEFAULT_BANNED) -> Optional[str]:
    """Return the first banned C-builtin the assembly calls, or None if clean."""
    for line in asm.splitlines():
        s = line.strip()
        if s.startswith("call_c_closure"):
            name = s.split()[1] if len(s.split()) > 1 else ""
            if name in banned:
                return name
    return None


def run_offline(asm: str, workdir: str, seed: int = DEFAULT_SEED,
                timeout_s: int = DEFAULT_TIMEOUT_S) -> Tuple[str, bool]:
    """Run assembly on the VM offline+seeded. Return (combined_output, timed_out)."""
    asm_path = os.path.join(workdir, "run.asm")
    with open(asm_path, "w") as f:
        f.write(asm)
    env = dict(os.environ, PIELO_OFFLINE="1", PIELO_SEED=str(seed))
    try:
        proc = subprocess.run(
            [VM, asm_path],
            env=env,
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
        return proc.stdout + proc.stderr, False
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or "") + (e.stderr or "")
        return (out.decode() if isinstance(out, bytes) else out), True


def extract_outcome(output: str) -> List[str]:
    """The ordered list of `Stack top:` values -- the program's observable outcome."""
    return [m.strip() for m in _STACK_TOP.findall(output)]


def asm_reactive_deps(asm: str) -> List[Tuple[str, List[str]]]:
    """Parse `define_closure <name> <nargs> <arg...> <ndeps> <dep...>` lines.

    Note the arg names sit BETWEEN nargs and ndeps, e.g. `define_closure f 1 x 1 a`
    is f(x) with one dependency `a`. Returns [(closure_name, [dependency_names])].
    Used only for the secondary, non-gating 'did it actually use a reactive
    dependency' signal.
    """
    out = []
    for line in asm.splitlines():
        s = line.strip().split()
        if len(s) >= 3 and s[0] == "define_closure":
            try:
                nargs = int(s[2])
                ndeps_idx = 3 + nargs
                ndeps = int(s[ndeps_idx])
                deps = s[ndeps_idx + 1: ndeps_idx + 1 + ndeps]
            except (ValueError, IndexError):
                continue
            out.append((s[1], deps))
    return out


# --- the verifier ------------------------------------------------------------


def _trial_seed(seed: int, i: int) -> int:
    """Decorrelated, deterministic per-trial seed.

    Structured tuple seeds like (seed, i) can yield correlated first draws across
    trials (observed: a>=b in every trial for one base seed). A cheap integer mix
    decorrelates the streams and stays independent of PYTHONHASHSEED.
    """
    return (seed * 2654435761 + (i + 1) * 40503) & 0xFFFFFFFF


def verify(task, candidate: str, *, seed: int = DEFAULT_SEED,
           n_trials: Optional[int] = None,
           timeout_s: int = DEFAULT_TIMEOUT_S) -> Result:
    """Run `candidate` against `task` over N randomized trials. Pass iff ALL pass.

    Per trial the SAME harness-injected (prelude, postlude) is wrapped around both
    the candidate and the task's reference solution; the reference is the oracle, so
    we compare extracted outcomes rather than hand-computing expected output. The
    inputs (and, for reactivity tasks, the post-hoc interventions) are randomized
    per trial -- which is what makes output-hardcoding fail.
    """
    candidate = strip_code_fence(candidate)
    n = n_trials if n_trials is not None else task.n_trials
    trials: List[TrialResult] = []

    for i in range(n):
        rng = Random(_trial_seed(seed, i))
        prelude, postlude = task.make_trial(i, rng)
        tr = _run_trial(task, candidate, i, prelude, postlude, seed, timeout_s)
        trials.append(tr)
        if not tr.passed:
            return Result(task.id, False, f"trial {i}: {tr.reason}", trials)

    return Result(task.id, True, f"all {n} trials passed", trials)


def _run_trial(task, candidate, i, prelude, postlude, seed, timeout_s) -> TrialResult:
    fail = lambda reason: TrialResult(i, False, reason, prelude, postlude)

    with tempfile.TemporaryDirectory(prefix="pielo_rl_") as wd:
        # Reference is the oracle. If it ever fails to compile/run, that's a bug in
        # the task, not the candidate -- surface it loudly.
        ref_dir = os.path.join(wd, "ref")
        cand_dir = os.path.join(wd, "cand")
        os.makedirs(ref_dir)
        os.makedirs(cand_dir)

        try:
            ref_asm = compile_to_asm(compose(prelude, task.reference, postlude), ref_dir)
        except CompileError as e:
            return fail(f"REFERENCE failed to compile (task bug): {e}")

        try:
            cand_asm = compile_to_asm(compose(prelude, candidate, postlude), cand_dir)
        except CompileError as e:
            return fail(f"candidate failed to compile: {e}")

        hit = scan_banned(cand_asm, task.banned)
        if hit:
            return fail(f"candidate used banned builtin '{hit}'")

        ref_out, ref_to = run_offline(ref_asm, ref_dir, seed, timeout_s)
        if ref_to:
            return fail("REFERENCE timed out (task bug)")
        cand_out, cand_to = run_offline(cand_asm, cand_dir, seed, timeout_s)
        if cand_to:
            return fail("candidate timed out")

        ref_trace = extract_outcome(ref_out)
        cand_trace = extract_outcome(cand_out)

        # Secondary, non-gating signal.
        mech = None
        if task.mechanism:
            deps = [d for _, ds in asm_reactive_deps(cand_asm) for d in ds]
            mech = task.mechanism in deps

        tr = TrialResult(i, True, "ok", prelude, postlude, cand_trace, ref_trace, mech)
        if cand_trace != ref_trace:
            tr.passed = False
            tr.reason = f"outcome {cand_trace} != reference {ref_trace}"
        return tr


def verify_batch(task, candidates: List[str], *, seed: int = DEFAULT_SEED,
                 n_trials: Optional[int] = None) -> List[Result]:
    """Verify many candidates against one task. Pure per candidate -> parallelizable.

    Kept sequential for now; a later training loop can swap in a process pool
    unchanged because each verify() call uses its own isolated temp dirs and no
    shared state.
    """
    return [verify(task, c, seed=seed, n_trials=n_trials) for c in candidates]
