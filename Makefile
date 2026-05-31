# Top-level convenience Makefile. The Parser/ and VM/ subdirs each have their own
# Makefile; this just orchestrates them and exposes the test harnesses.
.PHONY: all parser vm test test-multi clean

all: parser vm

parser:
	$(MAKE) -C Parser

vm:
	$(MAKE) -C VM

# Offline regression suite. See tests/README.md (does NOT cover networking).
# The harness builds what it needs (VM with tracing off), so no build prereq here.
test:
	bash tests/run_tests.sh

# Multi-robot (networked) suite -- opt-in, deliberately NOT part of the fast `make
# test`. Spins up the router + several VM processes over UDP and asserts invariants
# (propagation, stigmergy barrier, cross-robot reactivity, last-writer-wins). The
# harness builds its own binaries; see tests/README.md.
test-multi:
	bash tests/run_multi_tests.sh

clean:
	$(MAKE) -C Parser clean
	$(MAKE) -C VM clean
