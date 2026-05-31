#include "comparisonJumps.h"
#include "../vm.h"
using namespace PiELo;

void eql(){
    if(stack.size() >= 2){
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        if((a.getType() == NIL && b.getType() != NIL) || (a.getType() != NIL && b.getType() == NIL)){
            stack.push(0);
        } else if(a.getType() == NIL && b.getType() == NIL){
            stack.push(1);
        } else if((a.getType() == PIELO_CLOSURE || b.getType() == PIELO_CLOSURE) || (a.getType() == C_CLOSURE || b.getType() == C_CLOSURE)){
            // check for closure, comparing closures should be illegal
            throw InvalidTypeForOperationException("EQL", "CLOSURE");
            state = ERROR;
        }
        else if(a.getType() == FLOAT && b.getType() == INT){
            float conversion_value = static_cast<float>(b.getIntValue());
            stack.push(a.getFloatValue() == conversion_value ? 1 : 0);
        } else if(a.getType() == INT && b.getType() == FLOAT){
            float conversion_value = static_cast<float>(a.getIntValue());
            stack.push(conversion_value == b.getFloatValue() ? 1 : 0);
        } else if(a.getType() == FLOAT && b.getType() == FLOAT){
            stack.push(a.getFloatValue() == b.getFloatValue() ? 1 : 0);
        } else {
            stack.push(a.getIntValue() == b.getIntValue() ? 1 : 0);
        }
    } else {
        throw ShortOnElementsOnStackException("EQL");
        state = ERROR;
    }
}

void neql(){
    if(stack.size() >= 2){
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        if((a.getType() == NIL && b.getType() != NIL) || (a.getType() != NIL && b.getType() == NIL)){
            stack.push(1);
        } else if(a.getType() == NIL && b.getType() == NIL){
            stack.push(0);
        } else if((a.getType() == PIELO_CLOSURE || b.getType() == PIELO_CLOSURE) || (a.getType() == C_CLOSURE || b.getType() == C_CLOSURE)){
            // check for closure, comparing closures should be illegal
            throw InvalidTypeForOperationException("EQL", "CLOSURE");
            state = ERROR;
        }
        else if(a.getType() == FLOAT && b.getType() == INT){
            float conversion_value = static_cast<float>(b.getIntValue());
            stack.push(a.getFloatValue() != conversion_value ? 1 : 0);
        } else if(a.getType() == INT && b.getType() == FLOAT){
            float conversion_value = static_cast<float>(a.getIntValue());
            stack.push(conversion_value != b.getFloatValue() ? 1 : 0);
        } else if(a.getType() == FLOAT && b.getType() == FLOAT){
            stack.push(a.getFloatValue() != b.getFloatValue() ? 1 : 0);
        } else {
            stack.push(a.getIntValue() != b.getIntValue() ? 1 : 0);
        }
    } else {
        throw ShortOnElementsOnStackException("NEQL");
        state = ERROR;
    }
}

void gt(){
    if(stack.size() >= 2){
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        if((a.getType() == NIL && b.getType() != NIL) || (a.getType() != NIL && b.getType() == NIL)){
            throw InvalidTypeForOperationException("GT", "NIL");
            state = ERROR;
        } else if(a.getType() == NIL && b.getType() == NIL){
            stack.push(0);
        } else if((a.getType() == PIELO_CLOSURE || b.getType() == PIELO_CLOSURE) || (a.getType() == C_CLOSURE || b.getType() == C_CLOSURE)){
            // check for closure, comparing closures should be illegal
            throw InvalidTypeForOperationException("GT", "CLOSURE");
            state = ERROR;
        }
        else if(a.getType() == FLOAT && b.getType() == INT){
            float conversion_value = static_cast<float>(b.getIntValue());
            stack.push(a.getFloatValue() > conversion_value ? 1 : 0);
        } else if(a.getType() == INT && b.getType() == FLOAT){
            float conversion_value = static_cast<float>(a.getIntValue());
            stack.push(conversion_value > b.getFloatValue() ? 1 : 0);
        } else if(a.getType() == FLOAT && b.getType() == FLOAT){
            stack.push(a.getFloatValue() > b.getFloatValue() ? 1 : 0);
        } else {
            stack.push(a.getIntValue() > b.getIntValue() ? 1 : 0);
        }
    } else {
        throw ShortOnElementsOnStackException("GT");
        state = ERROR;
    }
}

void gte(){
    if(stack.size() >= 2){
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        if((a.getType() == NIL && b.getType() != NIL) || (a.getType() != NIL && b.getType() == NIL)){
            throw InvalidTypeForOperationException("GTE", "NIL");
            state = ERROR;
        } else if(a.getType() == NIL && b.getType() == NIL){
            stack.push(1);
        } else if((a.getType() == PIELO_CLOSURE || b.getType() == PIELO_CLOSURE) || (a.getType() == C_CLOSURE || b.getType() == C_CLOSURE)){
            // check for closure, comparing closures should be illegal
            throw InvalidTypeForOperationException("GTE", "CLOSURE");
            state = ERROR;
        }
        else if(a.getType() == FLOAT && b.getType() == INT){
            float conversion_value = static_cast<float>(b.getIntValue());
            stack.push(a.getFloatValue() >= conversion_value ? 1 : 0);
        } else if(a.getType() == INT && b.getType() == FLOAT){
            float conversion_value = static_cast<float>(a.getIntValue());
            stack.push(conversion_value >= b.getFloatValue() ? 1 : 0);
        } else if(a.getType() == FLOAT && b.getType() == FLOAT){
            stack.push(a.getFloatValue() >= b.getFloatValue() ? 1 : 0);
        } else {
            stack.push(a.getIntValue() >= b.getIntValue() ? 1 : 0);
        }
    } else {
        throw ShortOnElementsOnStackException("GTE");
        state = ERROR;
    }
}

void lt(){
    if(stack.size() >= 2){
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        if((a.getType() == NIL && b.getType() != NIL) || (a.getType() != NIL && b.getType() == NIL)){
            throw InvalidTypeForOperationException("LT", "NIL");
            state = ERROR;
        } else if(a.getType() == NIL && b.getType() == NIL){
            stack.push(0);
        } 
        else if((a.getType() == PIELO_CLOSURE || b.getType() == PIELO_CLOSURE) || (a.getType() == C_CLOSURE || b.getType() == C_CLOSURE)){
            // check for closure, comparing closures should be illegal
            throw InvalidTypeForOperationException("LT", "CLOSURE");
            state = ERROR;
        }
        else if(a.getType() == FLOAT && b.getType() == INT){
            float conversion_value = static_cast<float>(b.getIntValue());
            stack.push(a.getFloatValue() < conversion_value ? 1 : 0);
        } else if(a.getType() == INT && b.getType() == FLOAT){
            float conversion_value = static_cast<float>(a.getIntValue());
            stack.push(conversion_value < b.getFloatValue() ? 1 : 0);
        } else if(a.getType() == FLOAT && b.getType() == FLOAT){
            stack.push(a.getFloatValue() < b.getFloatValue() ? 1 : 0);
        } else {
            stack.push(a.getIntValue() < b.getIntValue() ? 1 : 0);
        }
    } else {
        throw ShortOnElementsOnStackException("LT");
        state = ERROR;
    }
}

void lte(){
    if(stack.size() >= 2){
        Variable a = stack.top(); stack.pop();
        Variable b = stack.top(); stack.pop();

        if((a.getType() == NIL && b.getType() != NIL) || (a.getType() != NIL && b.getType() == NIL)){
            throw InvalidTypeForOperationException("LTE", "NIL");
            state = ERROR;
        } else if(a.getType() == NIL && b.getType() == NIL){
            stack.push(1);
        } 
        else if((a.getType() == PIELO_CLOSURE || b.getType() == PIELO_CLOSURE) || (a.getType() == C_CLOSURE || b.getType() == C_CLOSURE)){
            // check for closure, comparing closures should be illegal
            throw InvalidTypeForOperationException("LTE", "CLOSURE");
            state = ERROR;
        }
        else if(a.getType() == FLOAT && b.getType() == INT){
            float conversion_value = static_cast<float>(b.getIntValue());
            stack.push(a.getFloatValue() <= conversion_value ? 1 : 0);
        } else if(a.getType() == INT && b.getType() == FLOAT){
            float conversion_value = static_cast<float>(a.getIntValue());
            stack.push(conversion_value <= b.getFloatValue() ? 1 : 0);
        } else if(a.getType() == FLOAT && b.getType() == FLOAT){
            stack.push(a.getFloatValue() <= b.getFloatValue() ? 1 : 0);
        } else {
            stack.push(a.getIntValue() <= b.getIntValue() ? 1 : 0);
        }
    } else {
        throw ShortOnElementsOnStackException("LTE");
        state = ERROR;
    }
}

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
        state = ERROR;
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
        state = ERROR;
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
        state = ERROR;
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
        state = ERROR;
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
            state = ERROR;
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
            state = ERROR;
        }
    }
}
