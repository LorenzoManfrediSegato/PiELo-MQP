#pragma once
#include <map>
#include <vector>
#include <string>
#include <stack>
#include "exceptions.h"
#include "instructionHandler.h"
#include <algorithm>
#include <variant>
#include <uuid/uuid.h>

namespace PiELo {
    enum VMState {
        READY,
        DONE,
        ERROR
    };

    enum Type {
        NIL,
        PIELO_CLOSURE,
        C_CLOSURE,
        FLOAT,
        INT,
        NAME
    };

    Type stringToType(std::string s);

    // Temporary typedef for however we store code
    typedef size_t codePtr;

    class Variable;

    typedef std::map<std::string, Variable> symbolTable;

    typedef Variable (*funp) (void);



    extern int robotID;
    typedef timeval timestamp_t;
    class VariableData;

    class VariableData {
        public:
        union {
            int asInt;
            float asFloat;
            size_t asClosureIndex;
            funp asFunctionPointer;
        };
        Type type = NIL;

        VariableData(int i) {asInt = i; type = INT;}

        VariableData(float f) {asFloat = f; type = FLOAT;}

        VariableData(size_t s) {asClosureIndex = s; type = PIELO_CLOSURE;}

        VariableData(funp f) {asFunctionPointer = f; type = C_CLOSURE;}

        VariableData() {type=NIL;}

        Type getType() {return type;}

        std::string getTypeAsString() {
            switch (type) {
                case NIL: return "NIL";
                case PIELO_CLOSURE: return "PIELO_CLOSURE";
                case C_CLOSURE: return "C_CLOSURE";
                case FLOAT: return "FLOAT";
                case INT: return "INT";
                case NAME: return "NAME";
                default: return "invalid type";
            }
        }

        void print();
    };

    struct ClosureData {
        // Pointer to code, however we decide to store that
        codePtr codePointer;
        symbolTable localSymbolTable; // Should this be a pointer? Probably
        std::vector<std::string> argNames;
        std::vector<std::string> dependencies;
        std::vector<size_t> dependants;
        VariableData cachedValue;
        bool marked = false; // For garbage collection
    };

    class ClosureMap : public std::map<size_t, ClosureData> {
        private:
            size_t headOfList = 0;
        public:
            void push_back(ClosureData& c);
            size_t getHeadOfList() {return headOfList;}
    };

    extern ClosureMap closureList;

    // A single bytecode cell: either an opcode or one of its inline arguments.
    //
    // Storage is a std::variant, so copy / move / destruction are all
    // compiler-generated and correct by construction -- no hand-rolled
    // copy-ctor / operator= / destructor juggling raw new/delete (that hand
    // management is what produced the two Phase-2 memory-corruption bugs: the
    // operator= switch fall-through and the non-POD malloc).
    //
    // `type` is kept as the authoritative discriminator because three logically
    // distinct cell kinds -- STRING, NAME, and LOCATION -- all share the same
    // std::string payload and can only be told apart by the tag. The assembler's
    // label-resolution pass also relies on retagging a STRING cell to LOCATION
    // in place (assemblyLoader.cpp), which is just a `type` change with the
    // string payload untouched. The accessors below preserve the previous
    // pointer-returning API (asString()/asLocation()/asClosure()) so call sites
    // are unchanged apart from the trailing ().
    struct opCodeInstructionOrArgument {
        enum Type {
            INSTRUCTION,
            FLOAT,
            INT,
            STRING,
            NIL,
            PIELO_CLOSURE,
            C_CLOSURE,
            NAME,
            LOCATION
        } type;

        // monostate covers NIL / C_CLOSURE (no payload). STRING/NAME/LOCATION all
        // use the std::string alternative, disambiguated by `type`.
        std::variant<std::monostate, Instruction, float, int, std::string, ClosureData> value;

        opCodeInstructionOrArgument(int value_i) : type(INT), value(value_i) {}

        opCodeInstructionOrArgument(float f) : type(FLOAT), value(f) {}

        opCodeInstructionOrArgument(Instruction instruction) : type(INSTRUCTION), value(instruction) {}

        opCodeInstructionOrArgument(codePtr codePointer, std::vector<std::string> dependencies, std::vector<std::string> args, std::vector<PiELo::Type> /*argTypes*/)
            : type(PIELO_CLOSURE), value(ClosureData{}) {
            ClosureData& c = std::get<ClosureData>(value);
            c.codePointer = codePointer;
            c.argNames = args;
            c.dependencies = dependencies;
        }

        opCodeInstructionOrArgument(ClosureData closureData) : type(PIELO_CLOSURE), value(std::move(closureData)) {}

        opCodeInstructionOrArgument(const std::string s) : type(STRING), value(s) {}

        std::string getTypeAsString() const {
            switch (type) {
                case NIL: return "NIL";
                case PIELO_CLOSURE: return "PIELO_CLOSURE";
                case C_CLOSURE: return "C_CLOSURE";
                case FLOAT: return "FLOAT";
                case INT: return "INT";
                case NAME: return "NAME";
                case INSTRUCTION: return "INSTRUCTION";
                case STRING: return "STRING";
                case LOCATION: return "LOCATION";
                default: return "invalid type";
            }
        }

        // Accessors preserving the previous field-like API. The string-bearing
        // kinds (STRING/NAME/LOCATION) all read out of the single string
        // alternative; `asString()` / `asLocation()` are the same storage under
        // different tags, matching the old union aliasing.
        Instruction asInstruction() const { return std::get<Instruction>(value); }
        std::string* asString() { return &std::get<std::string>(value); }
        const std::string* asString() const { return &std::get<std::string>(value); }
        std::string* asLocation() { return &std::get<std::string>(value); }
        const std::string* asLocation() const { return &std::get<std::string>(value); }
        ClosureData* asClosure() { return &std::get<ClosureData>(value); }
        const ClosureData* asClosure() const { return &std::get<ClosureData>(value); }

        float getFloatFromMemory() const {
            if (type != FLOAT) throw InvalidTypeAccessException("FLOAT", getTypeAsString());
            return std::get<float>(value);
        }

        int getIntFromMemory() const{
            if (type != INT) throw InvalidTypeAccessException("INT", getTypeAsString());
            return std::get<int>(value);
        }

        std::string* getNameValueFromMemory() {
            if (type != NAME) throw InvalidTypeAccessException("NAME", getTypeAsString());
            return &std::get<std::string>(value);
        }
    };

    

    struct Tag {
        std::string tagName;
    };

    class Variable {
    private:
        VariableData data;
        std::map<int, VariableData>::iterator iter;
        // VariableData data;
        
    public:

        std::map<int, VariableData> stigmergyData;
        bool changed = 0;
        bool isStigmergy = 0;
        // List of indices in the closure list
        std::vector<size_t> dependants;
        std::vector<Tag> tags;
        timestamp_t lastUpdated;

        Variable() {data.type = NIL;}

        Variable(int i) {data.type = INT; data.asInt = i;}

        Variable(float f) {data.type = FLOAT; data.asFloat = f;}

        Variable(size_t s) {data.type = PIELO_CLOSURE; data.asClosureIndex = s;}

        Variable(funp f) {data.type = C_CLOSURE; data.asFunctionPointer = f;}

        Variable(VariableData v) {data = v;}

        std::string getTypeAsString() {
            return data.getTypeAsString();
        }

        float getFloatValue() {
            stigmergyData.begin();
            if (isStigmergy) throw std::runtime_error("Tried to access stigmergy variable as float");
            if (data.type != FLOAT) throw InvalidTypeAccessException("FLOAT", getTypeAsString());
            return data.asFloat;
        }

        int getIntValue() {
            if (isStigmergy) throw std::runtime_error("Tried to access stigmergy variable as int");
            if (data.type != INT) throw InvalidTypeAccessException("INT", getTypeAsString());
            return data.asInt;
        }

        size_t getClosureIndex() {
            if (isStigmergy) throw std::runtime_error("Tried to access stigmergy variable as closure index");
            if (data.type != PIELO_CLOSURE) throw InvalidTypeAccessException("PIELO_CLOSURE", getTypeAsString());
            return data.asClosureIndex;
        }

        funp getFunctionPointer() {
            if (isStigmergy) throw std::runtime_error("Tried to access stigmergy variable as function pointer");
            if (data.type != C_CLOSURE) throw InvalidTypeAccessException("PIELO_CLOSURE", getTypeAsString());
            return data.asFunctionPointer;
        }

        // DO NOT USE TO EXTRACT DATA!
        VariableData getVariableData() {
            return data;
        }

        Type getType() {return data.type;}

        void mutateValue(float f) {if (isStigmergy) throw std::runtime_error("Tried to access stigmergy variable as function pointer"); data = f;}
        void mutateValue(int i) {if (isStigmergy) throw std::runtime_error("Tried to access stigmergy variable as function pointer"); data = i;}
        void mutateValue(size_t s) {if (isStigmergy) throw std::runtime_error("Tried to access stigmergy variable as function pointer"); data = s;}
        void mutateValue(Variable v) {if (isStigmergy) throw std::runtime_error("Tried to access stigmergy variable as function pointer"); data = v.data;}

        // Throws an error if this variable is not stigmergy
        void ensureStig() {
            if (!isStigmergy) throw std::runtime_error("Tried use stigmergy function on non-stigmergy variable");
        }

        void updateStigValue(int id, VariableData data) {
            ensureStig();
            this->data.type = data.type;
            stigmergyData[id] = data;
        }

        void updateStigValue(int id, Variable v) {
            updateStigValue(id, v.data);
        }

        int getStigSize() {
            ensureStig();
            return stigmergyData.size();
        }

        VariableData nextIterValue();
        
        VariableData peekIterValue();

        bool isIterAtEnd() {
            ensureStig();
            return iter == stigmergyData.end();
        }

        void resetIter() {
            ensureStig();
            iter = stigmergyData.begin();
        }
        
        void print() {
            if(getType() == NIL){
                std::cout << "nil";
            } else if(getType() == INT){
                std::cout << "int " << getIntValue();
            } else if(getType() == FLOAT){
                std::cout << "float " << getFloatValue();
            } else if (getType() == PIELO_CLOSURE) {
                std::cout << "closure index: ";
                std::cout << getClosureIndex();
            }
        }
    };


    struct scopeData{
        symbolTable* scopeSymbolTable;
        codePtr codePointer;
        size_t closureIndex;
    };

    
    extern std::vector<opCodeInstructionOrArgument> bytecode;
    extern codePtr programCounter;

    extern symbolTable taggedTable;
    // Variables which are at the top level but not tagged
    extern symbolTable globalSymbolTable;
    extern std::stack<Variable> stack;


    extern std::stack<scopeData> returnAddrStack;
    
    extern std::map<std::string, Variable>* currentSymbolTable; 

    extern std::vector<Tag> robotTagList;

    extern size_t currentClosureIndex;

    extern VMState state;

    Variable* findVariable(std::string name);

    // Run one line of assembly code
    VMState step();

    // Take in a string filename. Expects a file of the format:
    // instruction <arguments> \n
    // Allows comments with #
    // Loads the instructions into the bytecode vector.
    VMState load(std::string filename);

    void registerFunction(std::string name, funp f);
}