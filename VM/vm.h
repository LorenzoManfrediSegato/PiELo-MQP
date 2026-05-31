#pragma once
#include <map>
#include <vector>
#include <string>
#include <stack>
#include "exceptions.h"
#include "instructionHandler.h"
#include <algorithm>
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

    struct opCodeInstructionOrArgument { // struct that helps handle the storing of values pushed by opcodes.
        enum {
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
        union {
            Instruction asInstruction;
            float asFloat;
            int asInt;
            std::string* asString;
            ClosureData* asClosure;
            std::string* asLocation;
        };
    
        opCodeInstructionOrArgument(int value_i) : type(INT) {asInt = value_i;}

        opCodeInstructionOrArgument(float f) : type(FLOAT) {asFloat = f;}

        opCodeInstructionOrArgument(Instruction instruction): type(INSTRUCTION) {asInstruction = instruction;}

        opCodeInstructionOrArgument(codePtr codePointer, std::vector<std::string> dependencies, std::vector<std::string> args, std::vector<PiELo::Type> /*argTypes*/) {
            // ClosureData is non-POD (holds vectors/strings/a symbol table), so it must
            // be `new`ed, not malloc'd, or its members are never constructed (UB).
            type = PIELO_CLOSURE;
            asClosure = new ClosureData;
            asClosure->codePointer = codePointer;
            asClosure->argNames = args;
            asClosure->dependencies = dependencies;
        }

        opCodeInstructionOrArgument(ClosureData closureData) {
            type = PIELO_CLOSURE;
            asClosure = new ClosureData;
            *asClosure = closureData;
        }

        //opCodeInstructionOrArgument(const std::string& value_s) : type(STRING) {value.asString = new std::string(value);}
        
        opCodeInstructionOrArgument(const std::string s) : type(STRING) {
            asString = new std::string(s);
        }

        ~opCodeInstructionOrArgument() {
            if (type == STRING) {
                // std::cout << "freeing string ptr: " << asString << std::endl;
                // std::cout << "had value " << *asString << std::endl;
                delete asString;
            } else if (type == PIELO_CLOSURE) {
                // printf("Freeing closure \n");
                delete asClosure;
            } else if (type == LOCATION) {
                delete asLocation;
            }
        }

        opCodeInstructionOrArgument(const opCodeInstructionOrArgument& other) {
            // printf("Copying!\n");
            if (this != &other) {
                type = other.type;
                switch (other.type) {
                    case INSTRUCTION: asInstruction = other.asInstruction; break;
                    case FLOAT: asFloat = other.asFloat; break;
                    case INT: asInt = other.asInt; break;
                    case STRING: asString = new std::string(*other.asString); break;
                    case NIL: break;
                    case PIELO_CLOSURE:
                        asClosure = new ClosureData();
                        *asClosure = *other.asClosure;
                        break;
                    case C_CLOSURE: break; // no C-closure payload in this union
                    case NAME: asString = new std::string(*other.asString); break;
                    case LOCATION: asLocation = new std::string(*other.asLocation); break;
                }
            }
        }

        opCodeInstructionOrArgument& operator= (const opCodeInstructionOrArgument& other) {
            // printf("operator=\n");
            if (this != &other) {
                type = other.type;
                switch (other.type) {
                    case INSTRUCTION: asInstruction = other.asInstruction; break;
                    case FLOAT: asFloat = other.asFloat; break;
                    case INT: asInt = other.asInt; break;
                    case STRING: asString = new std::string(*other.asString); break;
                    case NIL: break;
                    case PIELO_CLOSURE:
                        asClosure = new ClosureData;
                        *asClosure = *other.asClosure;
                        break; // without this, fell through into NAME and read the
                               // closure pointer as a std::string* -> memory corruption
                    case C_CLOSURE: break; // no C-closure payload in this union
                    case NAME: asString = new std::string(*other.asString); break;
                    case LOCATION: asLocation = new std::string(*other.asLocation); break;
                }
            }
            return *this;
        }
        
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
        
        float getFloatFromMemory() const {
            if (type != FLOAT) throw InvalidTypeAccessException("FLOAT", getTypeAsString());
            return asFloat;
        }

        int getIntFromMemory() const{
            if (type != INT) throw InvalidTypeAccessException("INT", getTypeAsString());
            return asInt;
        }

        // ClosureData* getClosureDataFromMemory() {
        //     if (type != PIELO_CLOSURE) throw InvalidTypeAccessException("PIELO_CLOSURE", getTypeAsString());
        //     return asClosure;
        // }
        
        std::string* getNameValueFromMemory() const{
            if (type != NAME) throw InvalidTypeAccessException("NAME", getTypeAsString());
            return asString;
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