#include "assemblyLoader.h"
#include <iostream>
#include <stdexcept>
#include <functional>
#include <numeric>

using namespace PiELo;
#include "debug.h"

void Parser::initHandlers() {
     instructionHandlers = {
        {"store", [&]() {Parser::handleStore();}},
        {"tag", [&]() {Parser::handleTag();}},
        {"load", [&]() {Parser::handleLoad();}},
        {"push", [&]() {Parser::handlePush();}},
        {"push_nil", [&]() { handleSimple(PUSH_NIL); }},
        {"pop", [&]() {handlePop();}},
        {"is_nil", [&]() { handleJump(IS_NIL); }},
        {"add", [&]() { handleArithmetic(ADD); }},
        {"sub", [&]() { handleArithmetic(SUB); }},
        {"mul", [&]() { handleArithmetic(MUL); }},
        {"div", [&]() { handleArithmetic(DIV); }},
        {"mod", [&]() { handleSimple(MOD); }},
        {"print", [&]() {debugPrintParser("Parsing: print" << std::endl); handleSimple(PRINT); }},
        {"eql", [&]() { handleSimple(EQL); }},
        {"neql", [&]() { handleSimple(NEQL); }},
        {"gt", [&]() { handleSimple(GT); }},
        {"gte", [&]() { handleSimple(GTE); }},
        {"lt", [&]() { handleSimple(LT); }},
        {"lte", [&]() { handleSimple(LTE); }},
        {"land", [&]() { handleSimple(LAND); }},
        {"lor", [&]() { handleSimple(LOR); }},
        {"lnot", [&]() { handleSimple(LNOT); }},
        {"jmp", [&]() { handleJump(JMP); }},
        {"jmp_if_zero", [&]() { handleJump(JMP_IF_ZERO); }},
        {"jmp_if_not_zero", [&]() { handleJump(JMP_IF_NOT_ZERO); }},
        {"func", [&]() { handleFunctionOrLabel("func"); }},
        {"label", [&]() { handleFunctionOrLabel("label"); }},
        {"end", [&]() {debugPrintParser("Parsing: end" << std::endl); handleSimple(END); }},
        {"define_closure", [&]() {debugPrintParser("Parsing: define_closure" << std::endl); handleDefineClosure(); }},
        {"call_closure", [&]() {debugPrintParser("parsing: call_closure" << std::endl); handleSimple(CALL_CLOSURE);}},
        {"ret_from_closure", [&]() {handleSimple(RET_FROM_CLOSURE);}},
        {"call_c_closure", [&]() {Parser::handleCallC();}},
        {"uncache", [&]() {handleSimple(UNCACHE);}},
        {"push_next_in_stig", [&]() {bytecode.push_back(PUSH_NEXT_IN_STIG);
                                        bytecode.push_back(parseNextString());}},
        {"is_iter_at_end", [&]() {bytecode.push_back(IS_ITER_AT_END);
                                    bytecode.push_back(parseNextString());}},
        {"reset_iter", [&]() {bytecode.push_back(RESET_ITER);
                                    bytecode.push_back(parseNextString());}},
        {"stig_size", [&]() {Parser::handleStigSize();}},
        {"#", [&]() { file.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); }},
        {"debug_print", [&]() {handleDebugPrint();}},
        {"spin", [&]() { handleSimple(SPIN); debugPrintParser("bytecode size after spin is " << bytecode.size() << std::endl);}}
    };
}

void Parser::load(std::string filename){
    file.open(filename);
    if (!file.is_open()){
        throw std::runtime_error("Error opening file: "+ filename);
    }
    initHandlers();
    std::string instruction;

    while (file >> instruction) {
        auto it = instructionHandlers.find(instruction);
        if(it != instructionHandlers.end()) {
            // call appropriate handler
            instructionHandlers[instruction]();
        }
        else {
            throwInvalidInstruction(instruction);
        }
    }

    // Go back through the bytecode and replace all location labels with their bytecode counter locations
    // Can't do this on the first pass because the labels might be defined after when they get used
    for (size_t i = 0; i < bytecode.size(); i++) {
        auto ins = bytecode[i];
        if (ins.type == ins.LOCATION) {
            // If the opcode is a location, swap it out for the bytecode location stored in labelledLocations
            auto it = labelledLocations.find(*ins.asLocation());
            if (it != labelledLocations.end()) {
                bytecode[i] = (int) it->second;
                debugPrintParser("Swapped location " << *ins.asLocation() << " for bytecode position " << it->second << std::endl);
            } else {
                throw std::runtime_error("Could not find labelled location " + *ins.asLocation());
            }
            
        }
    }

    // return bytecode;
}

void Parser::handleDebugPrint() {
    std::string line;
    std::getline(file, line);
    bytecode.push_back(DEBUG_PRINT);
    bytecode.push_back(line);
}

void Parser::handlePush() {
    std::string type;
    file >> type;

    if (type == "i") {
        bytecode.push_back(PUSHI);
        bytecode.push_back(parseNextInt());
        debugPrintParser("parsed: push int " << bytecode.at(bytecode.size() - 1).getIntFromMemory() << std::endl);
    }
    else if (type == "f") {
        bytecode.push_back(PUSHF);
        bytecode.push_back(parseNextFloat());
    }
    else if (type == "s") {
        bytecode.push_back(PUSHS);
        bytecode.push_back(parseNextString());
    }
    else {
        throwInvalidInstruction("push " + type);
    }

}

void Parser::handleStore() {
    std::string type;
    file >> type;
    
    if (type == "local") {
        bytecode.push_back(STORE_LOCAL);
        bytecode.push_back(parseNextString()); // var name
    } else if (type == "global") {
        bytecode.push_back(STORE_GLOBAL);
        bytecode.push_back(parseNextString()); // var name
    } else if (type == "tagged") {
        bytecode.push_back(STORE_TAGGED);
        std::string name = parseNextString();
        debugPrintParser(" parsed: store tag name " << name << std::endl);
        bytecode.push_back(name);  // var name
    } else if (type == "stig") {
        bytecode.push_back(STORE_STIG);
        bytecode.push_back(parseNextString());
    }
    else {
        throwInvalidInstruction("store " + type);
    }
}


void Parser::handleTag() {
    std::string type;
    file >> type;

    if (type == "variable") {
        bytecode.push_back(TAG_VARIABLE);
        std::string name = parseNextString();
        std::string tag = parseNextString();
        debugPrintParser("Parsed tag variable " << name << " with tag " << tag << std::endl);
        bytecode.push_back(name);  // variable name
        bytecode.push_back(tag);  // tag name
    } else if (type == "robot") {
        bytecode.push_back(TAG_ROBOT);
        bytecode.push_back(parseNextString());  // tag name
    } else {
        throwInvalidInstruction("tag " + type);
    }
}

void Parser::handleLoad() {
    debugPrintParser("parser: Handling load" << std::endl);
    bytecode.push_back(LOAD_TO_STACK);
    // debugPrintParser(("1\n");
    // opCodeInstructionOrArgument name = parseNextString();
    // opCodeInstructionOrArgument name2 = name;
    // debugPrintParser("name str: " << name.asString() << " name2 str: " << name2.asString() << std::endl;
    bytecode.push_back(parseNextString());  // variable name
    debugPrintParser(" Pushed type " << bytecode[bytecode.size() - 1].type << std::endl);
    debugPrintParser(" value " << *bytecode[bytecode.size()-1].asString() << std::endl);
    // debugPrintParser(("2\n");
}

void Parser::handlePop() {
    bytecode.push_back(POP);
}

void Parser::handleArithmetic(const Instruction opcode) {
    bytecode.push_back(opcode);
}

void Parser::handleSimple(const Instruction opcode) {
    bytecode.push_back(opcode);
}

// `type` ("func" / "label") is currently unused -- funcs and labels are handled
// identically -- but kept at the call sites to document the distinction.
void Parser::handleFunctionOrLabel(const std::string& /*type*/) {
    int32_t position = bytecode.size();
    std::string locName = parseNextString();
    debugPrintParser("Parsed label " << locName << " at pos " << position << std::endl);
    labelledLocations[locName] = position;
}

void Parser::handleJump(const Instruction opcode) {
    bytecode.push_back(opcode);
    std::string name = parseNextString();
    try {
        bytecode.push_back(std::stoi(name));
    } catch (...) {
        opCodeInstructionOrArgument locName = name;
        locName.type = locName.LOCATION;
        bytecode.push_back(locName);
    }
    
    // auto it = labelledLocations.find(name);
    // if (it != labelledLocations.end()) {
    //     bytecode.push_back((int) it->second);  // label
    // } else {
    //     throw std::runtime_error("Invalid label name " + name);
    // }
}

void Parser::handleDefineClosure() {
    // bytecode.push_back(DEFINE_CLOSURE);
    ClosureData closure;

    std::string name = parseNextString();
    // bytecode.push_back(name);


    int numArgs = parseNextInt();
    debugPrintParser("Defineclosure: num args: " << numArgs << std::endl);
    
    for (int i = 0; i < numArgs; i++) {
        closure.argNames.push_back(parseNextString());
    }

    debugPrintParser("Done with args. " << std::endl);
    int numDependencies = parseNextInt();
    debugPrintParser("num deps: " << numDependencies << std::endl);
    for (int i = 0; i < numDependencies; i++) {
        debugPrintParser("Parsing dep." << std::endl);
        closure.dependencies.push_back(parseNextString());
    }
    debugPrintParser("Done with deps " << std::endl);

    closure.codePointer = bytecode.size() - 1;

    // bytecode.push_back(closure);
    defineClosure(name, closure);
    debugPrintParser("Done with defineClosure" << std::endl);
}

void Parser::handleCallC() {
    bytecode.push_back(CALL_C_CLOSURE);
    bytecode.push_back(parseNextString());
}

void Parser::handleStigSize() {
    bytecode.push_back(STIG_SIZE);
    bytecode.push_back(parseNextString());
}

int Parser::parseNextInt() {
    int value;
    file >> value;
    return value;
}

float Parser::parseNextFloat() {
    float value;
    file >> value;
    return value;
}

std::string Parser::parseNextString() {
    std::string value;
    file >> value;
    // debugPrintParser("parsed next string " << value << " addr: " << &value <<  std::endl);
    return value;
}

void Parser::throwInvalidInstruction(const std::string& instruction) {
    throw std::runtime_error("Invalid instruction: " + instruction);
}
