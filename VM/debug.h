#pragma once
#include <iostream>

// Consolidated debug-tracing macros for the VM.
//
// These used to be copy-pasted, byte-for-byte, at the top of every VM source
// file that wanted tracing (instructionHandler.cpp, parser.cpp, and the
// instructions/*.cpp). They live here once now.
//
// There are deliberately TWO macros gated on TWO independent compile-time
// switches (set in VM/Makefile, which turns DEBUG_* into -D __DEBUG_*__):
//
//   debugPrint(e)        -- per-opcode VM execution tracing. Gated on
//                           __DEBUG_INSTRUCTIONS__ (default ON in the Makefile).
//   debugPrintParser(e)  -- assembly-loader (VM/parser.cpp) tracing. Gated on
//                           __DEBUG_PARSER__ (default OFF in the Makefile).
//
// They are kept separate on purpose: turning on opcode tracing must NOT also
// turn on the (much chattier) parser tracing, and vice versa. Merging them onto
// one flag would change that behavior.
//
// NOTE: the macro body carries its own trailing ';', so call sites may omit it
// (e.g. `debugPrint("x" << y << std::endl)`). When the gate is off the macro
// expands to nothing. This matches the original copy-pasted definitions exactly.

#ifdef __DEBUG_INSTRUCTIONS__
#define debugPrint(e) std::cout << e;
#else
#define debugPrint(e)
#endif

#ifdef __DEBUG_PARSER__
#define debugPrintParser(e) std::cout << e;
#else
#define debugPrintParser(e)
#endif
