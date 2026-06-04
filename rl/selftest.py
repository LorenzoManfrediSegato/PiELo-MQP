#!/usr/bin/env python3
"""
Self-validation for the verifier -- the "a test that can't fail is worse than
no test" discipline applied to the CHECKER, not to any candidate.

For every task it asserts:
  * the reference solution PASSES all trials, and
  * every catalogued decoy (each a known wrong answer / reward-hack) FAILS.

A reference that fails means the task or the toolchain is broken; a decoy that
passes means the checker is gameable by a hack we already know about -- both are
hard failures here. This is also the dry-run that proves the verifier is wired to
the real Parser+VM before any model is put in the loop.

Run:  python3 rl/selftest.py        (exit 0 iff everything holds)
"""

from __future__ import annotations

import sys

from tasks import TASKS
from verify import verify


def main() -> int:
    failures = []
    print(f"Self-validating {len(TASKS)} tasks against the real Parser+VM "
          f"(reference must pass, decoys must fail)\n")

    for task in TASKS:
        print(f"== {task.id} [{task.tier}] ==")

        # 1. Reference must pass.
        r = verify(task, task.reference)
        if r.passed:
            print(f"  ok    reference passes ({r.reason})")
        else:
            print(f"  FAIL  reference did NOT pass: {r.reason}")
            failures.append(f"{task.id}: reference failed ({r.reason})")
            if r.first_failure:
                f = r.first_failure
                print(f"          cand={f.candidate_trace} ref={f.reference_trace}")

        # 2. Every decoy must fail.
        for d in task.decoys:
            r = verify(task, d.src)
            label = d.why[:60]
            if not r.passed:
                print(f"  ok    decoy fails as expected ({label})")
            else:
                print(f"  FAIL  decoy PASSED (checker is gameable!): {label}")
                failures.append(f"{task.id}: decoy passed -> {d.why}")

        # 3. Report the secondary mechanism signal on the reference, if any.
        if task.mechanism:
            r = verify(task, task.reference)
            present = r.trials[0].mechanism_present if r.trials else None
            print(f"  info  mechanism signal: reference depends on "
                  f"'{task.mechanism}' = {present} (secondary, non-gating)")
        print()

    print("=" * 50)
    if failures:
        print(f"SELFTEST FAILED ({len(failures)}):")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("SELFTEST PASSED: all references pass, all decoys fail.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
