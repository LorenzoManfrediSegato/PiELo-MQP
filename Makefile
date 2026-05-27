# Top-level convenience Makefile. The Parser/ and VM/ subdirs each have their own
# Makefile; this just orchestrates them and exposes the test harness.
.PHONY: all parser vm test clean

all: parser vm

parser:
	$(MAKE) -C Parser

vm:
	$(MAKE) -C VM

# Offline regression suite. See tests/README.md (does NOT cover networking).
# The harness builds what it needs (VM with tracing off), so no build prereq here.
test:
	bash tests/run_tests.sh

clean:
	$(MAKE) -C Parser clean
	$(MAKE) -C VM clean
