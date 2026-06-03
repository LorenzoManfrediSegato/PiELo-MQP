#include "instructionHandler.h"
// #include "arithmetic.h"
#include "instructions/arithmetic.h"
#include "instructions/closureInstructions.h"
#include "instructions/comparisonJumps.h"
#include "instructions/simpleInstructions.h"
#include "instructions/storeLoad.h"
#include "instructions/stigmergy.h"
#include "gc.h"
#include "vm.h"
#include "debug.h"

namespace PiELo{
    void handleInstruction(opCodeInstructionOrArgument op) {
        if (op.type != op.INSTRUCTION) {
            debugPrint("Attempted to run a type " << op.getTypeAsString() << " from pc " << programCounter << std::endl);
            debugPrint(*op.asString() << std::endl);
            throw std::runtime_error("Attempted to run non-instruction as instruction");
        }
        Instruction instruction = op.asInstruction();
        if (instruction != SPIN) {
            debugPrint("Running instruction " << instruction << std::endl);
        }
        std::string name;
        
        ClosureData closure;
        switch(instruction) {
            case DEFINE_CLOSURE:
                // Get closure name, closure data from bytecode
                debugPrint("running define_closure");
                name = *bytecode[++programCounter].asString();
                debugPrint(" Got name " << name);
                closure = *bytecode[++programCounter].asClosure();
                debugPrint(" got closure " << std::endl);
                defineClosure(name, closure);
                break;
            case CALL_CLOSURE:
                callClosure();
                break;
            
            case RET_FROM_CLOSURE:
                retFromClosure();
                GarbageCollector::collectGarbage();
                break;

            case RERUN_CLOSURE:
                rerunClosure(bytecode[++programCounter].getIntFromMemory());
                break;
            
            case CALL_C_CLOSURE:
                name = *bytecode[++programCounter].asString();
                debugPrint("Calling c closure: " << name);
                stack.push(taggedTable[name].getFunctionPointer()());
                break;
            
            case STORE_LOCAL:
                storeLocal(*bytecode[++programCounter].asString());
                break;
            case STORE_GLOBAL:
                storeGlobal(*bytecode[++programCounter].asString());
                break;
            case STORE_TAGGED:
                name = *bytecode[++programCounter].asString();
                debugPrint("instructionHandler: store name: " << name << std::endl);
                storeTagged(name);
                break;
            case STORE_STIG:
                name = *bytecode[++programCounter].asString();
                debugPrint("instructionHandler: store stig name: " << name << std::endl);
                storeStig(name);
                break;
            case PUSH_NEXT_IN_STIG:
                name = *bytecode[++programCounter].asString();
                debugPrint("instructionHandler: pushing next element of name: " << name << std::endl);
                pushNextElementOfStig(name);
                break;
            case IS_ITER_AT_END:
                name = *bytecode[++programCounter].asString();
                debugPrint("instructionHandler: is iter at end of name: " << name << std::endl);
                isIterAtEnd(name);
                break;
            case RESET_ITER:
                name = *bytecode[++programCounter].asString();
                debugPrint("instructionHandler: resetting iter for name: " << name << std::endl);
                resetIter(name);
                break;
            case STIG_SIZE:
                name = *bytecode[++programCounter].asString();
                stigSize(name);
                break;
            case TAG_VARIABLE:
                name = *bytecode[++programCounter].asString();
                tagVariable(name, *bytecode[++programCounter].asString());
                break;

            case TAG_ROBOT:
                tagRobot(*bytecode[programCounter++].asString());
                break;

            case LOAD_TO_STACK:
                // printf("Loading!\n");
                // std::cout << " symbol table has: " << std::endl;
                // for (auto it : *currentSymbolTable) {
                //     std::cout << "  " << it.first << ":";
                //     it.second.print();
                //     std::cout << std::endl;
                // }
                // std::cout << "Bytecode size: " << bytecode.size() << " pc: " << programCounter << std::endl;
                // std::cout << "type: " << bytecode[programCounter].type << std::endl;
                name = *bytecode[++programCounter].asString();
                // printf("Got name\n");
                loadToStack(name);
                break;

            case PRINT:
                std::cout << "Stack top: ";
                if(stack.top().getType() == NIL){
                    std::cout << "nil";
                } else if(stack.top().getType() == INT){
                    std::cout << stack.top().getIntValue();
                } else if(stack.top().getType() == FLOAT){
                    std::cout << stack.top().getFloatValue();
                } else if (stack.top().getType() == PIELO_CLOSURE) {
                    std::cout << " cached value: ";
                    closureList[stack.top().getClosureIndex()].cachedValue.print();
                }
                std::cout << std::endl;
                break;

            case DEBUG_PRINT:
                std::cout << *bytecode[++programCounter].asString() << std::endl;
                break;

            case NOP:
                // do nothing
                break;
            
            case END:
			    state = DONE;
                printf("Ran end.\n");
                break;

            case PUSH_NIL:
                pushNil();
                break;
            case PUSHI:
                pushInt();
                break;
            case PUSHF:
                pushFloat();
                break;
            case UNCACHE:
                uncache();
                break;
            case POP:
                pop();
                break;
            case IS_NIL:
                isNil();
                break;
            case ADD:
                add();
                break;
            case SUB:
                sub();
                break;
            case MUL:
                mul();
                break;
            case DIV:
                div();
                break;
            case MOD:
                mod();
                break;
            case DUP:
                dup();
                break;
            case SWAP:
                swap();
                break;
            case EQL:
                eql();
                break;
            case NEQL:
                neql();
                break;
            case GT:
                gt();
                break;
            case GTE:
                gte();
                break;
            case LT:
                lt();
                break;
            case LTE:
                lte();
                break;
            case LAND:
                land();
                break;
            case LOR:
                lor();
                break;
            case LNOT:
                lnot();
                break;
            case JMP:
                jump();
                break;
            case JMP_IF_ZERO:
                jump_if_zero();
                break;
            case JMP_IF_NOT_ZERO:
                jump_if_not_zero();
                break;
            case SPIN:
                programCounter--;
                break;
            default:
                std::cout << "Attempted to run unimplemented instruction " << instruction << std::endl;
                throw std::runtime_error("Unimplemented instruction! ");
                break;
        }
    }
}