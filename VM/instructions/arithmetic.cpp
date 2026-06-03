#include "arithmetic.h"
#include "../vm.h"

using namespace PiELo;

namespace {
    // The four binary arithmetic ops differ only in the scalar operation applied.
    // Operands are popped a=top, b=second-from-top, and every original branch
    // computed b OP a (add/mul are commutative so b+a==a+b and b*a==a*b; sub/div
    // were already written as b-a / b/a). Result is INT only when both operands
    // are INT, otherwise FLOAT -- matching the original per-op type rules.
    enum class ArithOp { ADD, SUB, MUL, DIV };

    float applyFloat(ArithOp op, float b, float a) {
        switch (op) {
            case ArithOp::ADD: return b + a;
            case ArithOp::SUB: return b - a;
            case ArithOp::MUL: return b * a;
            case ArithOp::DIV: return b / a;
        }
        return 0.0f; // unreachable; all enum cases return above
    }

    int applyInt(ArithOp op, int b, int a) {
        switch (op) {
            case ArithOp::ADD: return b + a;
            case ArithOp::SUB: return b - a;
            case ArithOp::MUL: return b * a;
            case ArithOp::DIV: return b / a; // integer division, as before
        }
        return 0; // unreachable; all enum cases return above
    }

    void binaryArith(ArithOp op, const std::string& name) {
        if (stack.size() < 2) throw ShortOnElementsOnStackException(name);

        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        if (a.getType() == NAME || b.getType() == NAME) {
            throw InvalidTypeForOperationException(name, "NAME");
        } else if (a.getType() == NIL || b.getType() == NIL) {
            throw InvalidTypeForOperationException(name, "NIL");
        } else if (op == ArithOp::DIV &&
                   ((a.getType() == INT && a.getIntValue() == 0) ||
                    (a.getType() == FLOAT && a.getFloatValue() == 0.0f))) {
            throw DivisionByZeroException();
        } else if (a.getType() == FLOAT && b.getType() == INT) {
            stack.push(Variable(applyFloat(op, static_cast<float>(b.getIntValue()), a.getFloatValue())));
        } else if (a.getType() == INT && b.getType() == FLOAT) {
            stack.push(Variable(applyFloat(op, b.getFloatValue(), static_cast<float>(a.getIntValue()))));
        } else if (a.getType() == INT && b.getType() == INT) {
            stack.push(Variable(applyInt(op, b.getIntValue(), a.getIntValue())));
        } else if (a.getType() == FLOAT && b.getType() == FLOAT) {
            stack.push(Variable(applyFloat(op, b.getFloatValue(), a.getFloatValue())));
        } else if (a.getType() == PIELO_CLOSURE || b.getType() == PIELO_CLOSURE) {
            // One or both operands is a closure: substitute its cached value and
            // recurse. NOTE: this preserves the original push order (a_cv then
            // b_cv), which means the recursive call pops them SWAPPED (a_cv lands
            // as the second operand). For add/mul that is harmless; for sub/div it
            // inverts the operation. This is existing, untested behavior -- kept
            // verbatim here rather than "fixed" silently as part of a refactor.
            VariableData a_cv = (a.getType() == PIELO_CLOSURE)
                ? closureList[a.getClosureIndex()].cachedValue : a.getVariableData();
            VariableData b_cv = (b.getType() == PIELO_CLOSURE)
                ? closureList[b.getClosureIndex()].cachedValue : b.getVariableData();
            stack.push(a_cv);
            stack.push(b_cv);
            binaryArith(op, name);
        } else {
            throw InvalidTypeForOperationException(name, a.getTypeAsString() + "+" + b.getTypeAsString());
        }
    }
}

void add() { binaryArith(ArithOp::ADD, "ADD"); }
void sub() { binaryArith(ArithOp::SUB, "SUB"); } // b - a
void mul() { binaryArith(ArithOp::MUL, "MUL"); }
void div() { binaryArith(ArithOp::DIV, "DIV"); } // b / a

// mod is intentionally NOT routed through binaryArith: it is integer-only and
// handles the closure-operand combinations with its own (different) structure,
// so folding it in would add contortion without removing real duplication. Left
// as its own implementation; tidying its internal repetition is a separate step.
void mod(){
    if(stack.size() >= 2) {
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        VariableData a_cv;
        VariableData b_cv;

        if(a.getType() == PIELO_CLOSURE && b.getType() != PIELO_CLOSURE){

            a_cv = closureList[a.getClosureIndex()].cachedValue;

            if(a_cv.getType() != INT || b.getType() != INT){
                throw InvalidTypeForOperationException("MOD", "!= than INT. Only INT type is allowed.");
            } else if (a_cv.asInt == 0){
                throw DivisionByZeroException();
            } else {
                Variable result = b.getIntValue() % a_cv.asInt;
                stack.push(result);
            }

        }
        else if(a.getType() != PIELO_CLOSURE && b.getType() == PIELO_CLOSURE){

           b_cv = closureList[b.getClosureIndex()].cachedValue;

            if(b_cv.getType() != INT || a.getType() != INT){
                throw InvalidTypeForOperationException("MOD", "!= than INT. Only INT type is allowed.");
            } else if (a.getIntValue() == 0){
                throw DivisionByZeroException();
            }
            else {
                Variable result = b_cv.asInt % a.getIntValue();
                stack.push(result);
            }

        } else if(a.getType() == PIELO_CLOSURE && b.getType() == PIELO_CLOSURE){

            a_cv = closureList[a.getClosureIndex()].cachedValue;
            b_cv = closureList[b.getClosureIndex()].cachedValue;

            if(a_cv.getType() != INT || b_cv.getType() != INT){
                throw InvalidTypeForOperationException("MOD", "!= than INT. Only INT type is allowed.");
            } else if (a_cv.asInt == 0){
                throw DivisionByZeroException();
            } else {
                Variable result = b_cv.asInt % a_cv.asInt;
                stack.push(result);
            }

        } else if(a.getType() == INT || b.getType() == INT) {
            if (a.getIntValue() == 0){
                throw DivisionByZeroException();
            } else {
                Variable result = b.getIntValue() % a.getIntValue();
                stack.push(result);
            }
        } else {
            throw InvalidTypeForOperationException("MOD", "!= than INT. Only INT type is allowed.");
        }
    } else {
        throw ShortOnElementsOnStackException("MOD");
    }
}
