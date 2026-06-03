#include "comparisonJumps.h"
#include "../vm.h"
using namespace PiELo;

namespace {
    // The six comparison ops share one shape: pop a (top) and b (second), then
    // compute `a OP b` over INT/FLOAT with int->float promotion on mixed pairs.
    // They differ only in (1) the scalar comparator and (2) how they treat NIL
    // operands -- and the NIL policies genuinely diverge, which is exactly what a
    // careless merge would flatten, so they are made explicit parameters here:
    //
    //   op   | exactly-one NIL | both NIL | comparator
    //   -----+-----------------+----------+-----------
    //   EQL  | push 0          | push 1   | ==
    //   NEQL | push 1          | push 0   | !=
    //   GT   | throw           | push 0   | >
    //   GTE  | throw           | push 1   | >=
    //   LT   | throw           | push 0   | <
    //   LTE  | throw           | push 1   | <=
    //
    // `oneNilThrows` selects throw-vs-value for the exactly-one-NIL case; when it
    // is false, `oneNilResult` is pushed. `bothNilResult` covers both-NIL.
    template <typename Cmp>
    void compare(const std::string& name, Cmp cmp,
                 bool oneNilThrows, int oneNilResult, int bothNilResult) {
        if (stack.size() < 2) throw ShortOnElementsOnStackException(name);

        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        const bool aNil = a.getType() == NIL;
        const bool bNil = b.getType() == NIL;

        if (aNil != bNil) { // exactly one operand is NIL
            if (oneNilThrows) throw InvalidTypeForOperationException(name, "NIL");
            stack.push(oneNilResult);
        } else if (aNil && bNil) {
            stack.push(bothNilResult);
        } else if (a.getType() == PIELO_CLOSURE || b.getType() == PIELO_CLOSURE ||
                   a.getType() == C_CLOSURE   || b.getType() == C_CLOSURE) {
            // comparing closures is illegal
            throw InvalidTypeForOperationException(name, "CLOSURE");
        } else if (a.getType() == FLOAT && b.getType() == INT) {
            stack.push(cmp(a.getFloatValue(), static_cast<float>(b.getIntValue())) ? 1 : 0);
        } else if (a.getType() == INT && b.getType() == FLOAT) {
            stack.push(cmp(static_cast<float>(a.getIntValue()), b.getFloatValue()) ? 1 : 0);
        } else if (a.getType() == FLOAT && b.getType() == FLOAT) {
            stack.push(cmp(a.getFloatValue(), b.getFloatValue()) ? 1 : 0);
        } else {
            stack.push(cmp(a.getIntValue(), b.getIntValue()) ? 1 : 0);
        }
    }

    // Generic comparators so INT and FLOAT instantiations share one definition.
    struct EqOp  { template <typename T> bool operator()(T x, T y) const { return x == y; } };
    struct NeOp  { template <typename T> bool operator()(T x, T y) const { return x != y; } };
    struct GtOp  { template <typename T> bool operator()(T x, T y) const { return x >  y; } };
    struct GteOp { template <typename T> bool operator()(T x, T y) const { return x >= y; } };
    struct LtOp  { template <typename T> bool operator()(T x, T y) const { return x <  y; } };
    struct LteOp { template <typename T> bool operator()(T x, T y) const { return x <= y; } };
}

void eql()  { compare("EQL",  EqOp{},  /*oneNilThrows=*/false, /*oneNil=*/0, /*bothNil=*/1); }
void neql() { compare("NEQL", NeOp{},  /*oneNilThrows=*/false, /*oneNil=*/1, /*bothNil=*/0); }
void gt()   { compare("GT",   GtOp{},  /*oneNilThrows=*/true,  /*oneNil=*/0, /*bothNil=*/0); }
void gte()  { compare("GTE",  GteOp{}, /*oneNilThrows=*/true,  /*oneNil=*/0, /*bothNil=*/1); }
void lt()   { compare("LT",   LtOp{},  /*oneNilThrows=*/true,  /*oneNil=*/0, /*bothNil=*/0); }
void lte()  { compare("LTE",  LteOp{}, /*oneNilThrows=*/true,  /*oneNil=*/0, /*bothNil=*/1); }

bool convertVarToBool(Variable var) {
    bool boolVal = true;
    if (var.getType() == NIL) boolVal = false;
    else if (var.getType() == INT) {
        if (var.getIntValue() == 0) boolVal = false;
    } else if (var.getType() == FLOAT) {
        if (var.getFloatValue() == 0) boolVal = false;
    }
    return boolVal;
}

void land() {
    if(stack.size() >= 2){
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        bool aVal = convertVarToBool(a);
        bool bVal = convertVarToBool(b);
        if (aVal && bVal) stack.push(1);
        else stack.push(0);
    } else {
        throw ShortOnElementsOnStackException("LAND");
    }
}

void lor() {
    if(stack.size() >= 2){
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        bool aVal = convertVarToBool(a);
        bool bVal = convertVarToBool(b);
        if (aVal || bVal) stack.push(1);
        else stack.push(0);
    } else {
        throw ShortOnElementsOnStackException("LOR");
    }
}

void lnot() {
    if(stack.size() >= 1){
        Variable a = stack.top(); stack.pop();

        bool aVal = convertVarToBool(a);
        if (aVal) stack.push(0);
        else stack.push(1);
    } else {
        throw ShortOnElementsOnStackException("LOR");
    }
}


void jump(){
    programCounter++;
    if(bytecode.at(programCounter).getTypeAsString() == "INT"){
        int target_address = bytecode.at(programCounter).getIntFromMemory();
        // PC gets incremented by PiELo::step() finishing
        programCounter = target_address - 1;
    } else {
        throw AddressNotDecleredException();
    }
}

void jump_if_zero(){
    programCounter++;
    if (stack.size() < 1) throw ShortOnElementsOnStackException("jmp_if_zero");
    Variable top = stack.top();
    stack.pop();
    if((top.getType() == FLOAT && top.getFloatValue() == 0.0f) || (top.getType() == INT && top.getIntValue() == 0)){
        if(bytecode.at(programCounter).getTypeAsString() == "INT" && bytecode.size() > static_cast<size_t>(bytecode.at(programCounter).getIntFromMemory())){
            int target_address = bytecode.at(programCounter).getIntFromMemory();
            // PC gets incremented by PiELo::step() finishing
            programCounter = target_address - 1;
        } else {
            throw AddressNotDecleredException();
        }
    }
}

void jump_if_not_zero(){
    programCounter++;
    if (stack.size() < 1) throw ShortOnElementsOnStackException("jmp_if_not_zero");
    Variable top = stack.top();
    stack.pop();
    if((top.getType() == FLOAT && top.getFloatValue() != 0.0f) || (top.getType() == INT && top.getIntValue() != 0)){
        if(bytecode.at(programCounter).getTypeAsString() == "INT" && bytecode.size() > static_cast<size_t>(bytecode.at(programCounter).getIntFromMemory())){
            int target_address = bytecode.at(programCounter).getIntFromMemory();
            // PC gets incremented by PiELo::step() finishing
            programCounter = target_address - 1;
        } else {
            throw AddressNotDecleredException();
        }
    }
}
