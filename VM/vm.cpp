#include "vm.h"
#include "parser.h"
#include "networking.h"
#include "robotFunctions.h"
#include "gc.h"


namespace PiELo {
    Type stringToType(std::string s) {
        // Thanks to https://stackoverflow.com/questions/313970/how-to-convert-an-instance-of-stdstring-to-lower-case
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
        if (s == "nil") return NIL;
        else if (s == "pielo_closure") return PIELO_CLOSURE;
        else if (s == "c_closure") return C_CLOSURE;
        else if (s == "float") return FLOAT;
        else if (s == "int") return INT;
        else if (s == "name") return NAME;
        else throw InvalidTypeException();
    }

    std::vector<opCodeInstructionOrArgument> bytecode;
    codePtr programCounter = 0;

    symbolTable taggedTable;
    // Variables which are at the top level but not tagged
    symbolTable globalSymbolTable;
    ClosureMap closureList;
    std::stack<Variable> stack;


    std::stack<scopeData> returnAddrStack;
    
    std::map<std::string, Variable>* currentSymbolTable = &globalSymbolTable; 

    std::vector<Tag> robotTagList;

    size_t currentClosureIndex;

    VMState state;

    int robotID = -1;

    Parser parser;   


    VMState load(std::string filename) {
        if (initNetworking() != 0) return VMState::ERROR;
        parser.load(filename);
        return VMState::READY;
    }

    VMState step() {
        // TODO: move the robot functions to a registered c function?
        robot.updatePos();
        // std::cout << "At pc " << programCounter << std::endl;
        handleInstruction(bytecode[programCounter]);
        // Only poll the network at top level (not mid-evaluation). Processing a
        // message can trigger handleDependants, which jumps the PC into a reactive
        // closure; polling again before that closure returns would re-enter the
        // recompute and corrupt the operand stack (ret_from_closure leaves a closure
        // value behind, which then collides with the in-progress evaluation -- e.g.
        // `gt` receiving a CLOSURE). returnAddrStack is empty exactly at top level.
        if (returnAddrStack.empty()) checkForMessage();
        programCounter++;

        if (programCounter >= bytecode.size() || state == DONE) return VMState::DONE;
        return VMState::READY;
    }

    Variable* findVariable(std::string name) {
        try {
            return &currentSymbolTable->at(name);
        } catch (...) {}

        try {
            return &globalSymbolTable.at(name);
        } catch (...) {}

        try {
            return &taggedTable.at(name);
        } catch (...) {}

        throw std::runtime_error("Could not find variable " + name);
    }

    // For now, c closures are limited to no arguments, no calling pielo closures, nothing!
    void registerFunction(std::string name, funp f) {
        taggedTable[name] = f;
    }

    void VariableData::print() { 
        if(getType() == NIL){
            std::cout << "nil";
        } else if(getType() == INT){
            std::cout << "int " << asInt;
        } else if(getType() == FLOAT){
            std::cout << "float " << asFloat;
        } else if (getType() == PIELO_CLOSURE) {
            std::cout << "closure index: ";
            std::cout << asClosureIndex;
            std::cout << " cached value: ";
            closureList[asClosureIndex].cachedValue.print();
        }
    }

    VariableData Variable::nextIterValue() {
        ensureStig();
        VariableData data;
        if (iter != stigmergyData.end()) {
            // std::cout << " in here " << "with type " << iter->second.getTypeAsString() << std::endl;
            data = iter->second;
            iter++;
        }
        return data;
    }

    VariableData Variable::peekIterValue() {
        ensureStig();
        VariableData data;
        if (iter != stigmergyData.end()) {
            // std::cout << " peeking iter value " << "with type " << iter->second.getTypeAsString() << std::endl;
            data = iter->second;
        }
        return data;
    }

    void ClosureMap::push_back(ClosureData& c) {
        size_t insertLoc = headOfList++;
        #ifdef __DEBUG_INSTRUCTIONS__
            std::cout << "Inserting at index " << insertLoc << " symbol table size " << c.localSymbolTable.size() << std::endl;
        #endif
        this->insert(std::pair<size_t, ClosureData>(insertLoc, c));
        this->at(insertLoc).argNames = c.argNames;
        this->at(insertLoc).cachedValue = c.cachedValue;
        this->at(insertLoc).codePointer = c.codePointer;
        this->at(insertLoc).dependants = c.dependants;
        this->at(insertLoc).dependencies = c.dependencies;
        this->at(insertLoc).localSymbolTable = c.localSymbolTable;
        this->at(insertLoc).marked = c.marked;
    }
}
